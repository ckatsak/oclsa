#include <llvm/Pass.h>                // llvm::RegisterPass, llvm::FunctionPass
#include <llvm/IR/Function.h>         // llvm::Function
#include <llvm/IR/Module.h>           // llvm::Module
#include <llvm/IR/Instruction.h>      // llvm::Instruction(?)
#include <llvm/IR/Instructions.h>     // llvm::LoadInst, llvm::StoreInst
#include <llvm/Support/raw_ostream.h> // llvm::errs, llvm::raw_ostream
#include <llvm/PassAnalysisSupport.h> // llvm::AnalysisUsage, llvm::getAnalysis
#include <llvm/Analysis/LoopInfo.h> // llvm::LoopInfoWrapperPass,llvm::LoopInfo
#include <llvm/Support/Casting.h>     // llvm::dyn_cast

#include <string>        // std::string
#include <unordered_set> // std::unordered_set
#include <utility>       // std::pair
#include <vector>        // std::vector

// Binary, bitwise binary, vector and aggregate operations, as listed in:
// https://releases.llvm.org/7.0.1/docs/LangRef.html .
const std::unordered_set<std::string> BIN_OPS = {
  "add", "fadd", "sub", "fsub", "mul", "fmul", "udiv", "sdiv", "fdiv", "urem",
  "srem", "frem",
};
const std::unordered_set<std::string> BIT_BIN_OPS = {
  "shl", "lshr", "ashr", "and", "or", "xor",
};
const std::unordered_set<std::string> VEC_OPS = {
  "extractelement", "insertelement", "shufflevector",
};
const std::unordered_set<std::string> AGG_OPS = {
  "extractvalue", "insertvalue",
};

// It is assumed that OpenCL's `__local` and `__global` have been substituted
// by `__attribute__((address_space(X)))`, where X is 1 or 2, respectively.
// This way we can leverage LLVM's API to count stats about `__local` and
// `__global` memory accesses.
constexpr unsigned privateAddressSpace = 0;
constexpr unsigned localAddressSpace   = 1;
constexpr unsigned globalAddressSpace  = 2;

struct BasicBlockStatsData {
  BasicBlockStatsData()
      : OwnerLoop(nullptr),
        NumBinOps(0), NumBitBinOps(0), NumVecOps(0), NumAggOps(0),
        NumLoadOps(0), NumStoreOps(0), NumOtherOps(0), NumCallOps(0),
        NumGlobalMemAcc(0), NumLocalMemAcc(0), NumPrivateMemAcc(0) {}

  const llvm::Loop* OwnerLoop;

  unsigned NumBinOps;
  unsigned NumBitBinOps;
  unsigned NumVecOps;
  unsigned NumAggOps;
  unsigned NumLoadOps;
  unsigned NumStoreOps;
  unsigned NumCallOps;
  unsigned NumOtherOps;

  unsigned NumGlobalMemAcc;
  unsigned NumLocalMemAcc;
  unsigned NumPrivateMemAcc;
};
using BasicBlockStats = std::pair<const llvm::BasicBlock*,
                                  BasicBlockStatsData>;
using BasicBlocks = std::vector<BasicBlockStats>;

namespace {

// forward declarations
void evalInstruction(const llvm::Instruction*, BasicBlockStatsData&);
void debug_err_BasicBlockStatsData(BasicBlockStatsData);

// functionStats stores the stats for all functions in the Module
std::vector<BasicBlockStats> functionStats{};

struct oclsa : public llvm::FunctionPass {
  static char ID;

  oclsa() : llvm::FunctionPass(ID) {}

  void getAnalysisUsage(llvm::AnalysisUsage& AU) const override {
    AU.setPreservesAll(); // set by analyses that do not transform their input
                          // at all
    AU.setPreservesCFG(); // set by passes that do not:
                          //  1. add or remove BBs from the Function
                          //  2. modify terminator instructions in any way
    AU.addRequired<llvm::LoopInfoWrapperPass>();
  }

  bool runOnFunction(llvm::Function &CurrFunc) override {
    llvm::LoopInfo& LI = getAnalysis<llvm::LoopInfoWrapperPass>().getLoopInfo();

    for (llvm::BasicBlock& bb : CurrFunc) {
      BasicBlockStatsData bbsd{};

      bbsd.OwnerLoop = LI.getLoopFor(&bb);
      for (const llvm::Instruction& inst : bb) {
        evalInstruction(&inst, bbsd);
      }

      functionStats.emplace_back(&bb, bbsd);
      debug_err_BasicBlockStatsData(bbsd); // FIXME DEBUG PRINT
    }

    return false;
  }

  void print(llvm::raw_ostream& OS, const llvm::Module* CurrModule) const {
//    OS << "hello from print()\n"
//       << "\tCurrModule->getName() = " << CurrModule->getName() << '\n';
//    debug_err_BasicBlockStatsData(bbsd);
  }
}; // end struct oclsa

void checkAddrSpace(const unsigned addrSpaceID, BasicBlockStatsData &stats) {
  if (addrSpaceID == privateAddressSpace) {
    stats.NumPrivateMemAcc++;
  } else if (addrSpaceID == localAddressSpace) {
    stats.NumLocalMemAcc++;
  } else if (addrSpaceID == globalAddressSpace) {
    stats.NumGlobalMemAcc++;
  }
}

void evalInstruction(const llvm::Instruction* instr,
                     BasicBlockStatsData& stats) {
  //llvm::errs().write_escaped(instr->getOpcodeName()) << '\n'; // DEBUG
  std::string opName = instr->getOpcodeName();

  if (BIN_OPS.find(opName) != BIN_OPS.end()) {
    stats.NumBinOps++;
  } else if (BIT_BIN_OPS.find(opName) != BIT_BIN_OPS.end()) {
    stats.NumBitBinOps++;
  } else if (VEC_OPS.find(opName) != VEC_OPS.end()) {
    stats.NumVecOps++;
  } else if (AGG_OPS.find(opName) != AGG_OPS.end()) {
    stats.NumAggOps++;
  } else if (opName == "call") {
    stats.NumCallOps++;
  } else if (const llvm::LoadInst* li =
      llvm::dyn_cast<llvm::LoadInst>(instr)) {
    stats.NumLoadOps++;
    checkAddrSpace(li->getPointerAddressSpace(), stats);
  } else if (const llvm::StoreInst* si =
      llvm::dyn_cast<llvm::StoreInst>(instr)) {
    stats.NumStoreOps++;
    checkAddrSpace(si->getPointerAddressSpace(), stats);
  } else {
    stats.NumOtherOps++;
  }
}

void debug_err_BasicBlockStatsData(BasicBlockStatsData bbsd) {
  llvm::errs() << "\n\tBasicBlock:";
  llvm::errs() << "\n\t\tOwnerLoop        : " << bbsd.OwnerLoop;
  llvm::errs() << "\n\t\tNumBinOps        = " << bbsd.NumBinOps;
  llvm::errs() << "\n\t\tNumBitBinOps     = " << bbsd.NumBitBinOps;
  llvm::errs() << "\n\t\tNumVecOps        = " << bbsd.NumVecOps;
  llvm::errs() << "\n\t\tNumAggOps        = " << bbsd.NumAggOps;
  llvm::errs() << "\n\t\tNumLoadOps       = " << bbsd.NumLoadOps;
  llvm::errs() << "\n\t\tNumStoreOps      = " << bbsd.NumStoreOps;
  llvm::errs() << "\n\t\tNumCallOps       = " << bbsd.NumCallOps;
  llvm::errs() << "\n\t\tNumOtherOps      = " << bbsd.NumOtherOps;
  llvm::errs() << "\n\t\tNumLocalMemAcc   = " << bbsd.NumLocalMemAcc;
  llvm::errs() << "\n\t\tNumGlobalMemAcc  = " << bbsd.NumGlobalMemAcc;
  llvm::errs() << "\n\t\tNumPrivateMemAcc = " << bbsd.NumPrivateMemAcc;
  llvm::errs() << '\n';
}

} // anonymous namespace

char oclsa::ID = 0;
static llvm::RegisterPass<oclsa> oclsaRegister("oclsa",
                                               "Basic OpenCL static analysis",
                                               false,
                                               false);
