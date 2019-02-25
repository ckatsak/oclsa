add_library(LLVMoclsa MODULE
	Pass.cc
)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(LLVMoclsa PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
)
