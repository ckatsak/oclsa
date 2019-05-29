#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
    demofeatxtr.py

    Christos Katsakioris <ckatsak[at]gmail[dot]com>

    Requires python >= 3.5
"""

import logging
import os
import os.path
import subprocess
import tempfile
from typing import Dict

from flask import Flask, jsonify, make_response, request


CLANG_BITCHING_KILLER = """
#define __global __attribute__((address_space(1)))
#define __local  __attribute__((address_space(2)))

size_t get_local_id(uint);
size_t get_local_size(uint);
size_t get_global_id(uint);
size_t get_global_size(uint);
size_t get_num_groups(uint);
size_t get_group_id(uint);

"""
TMP_PATH = "/tmp"
CLANG_FRONTEND_CMD = (
        "clang -include $LIBCLC_HOME/generic/include/clc/clc.h -x cl "
        "-I $LIBCLC_HOME/generic/include -Dcl_clang_storage_class_specifiers "
        "-emit-llvm -c -O0 {0}.cl -o {0}.bc"
)
OCLSA_PASS_CMD = "opt -load /usr/lib/libLLVMoclsa.so -oclsa %s.bc"


def extract_features():
    """
    On success, it returns 200 OK along with a JSON object that describes the
    code features extracted from the provided kernel.
    On failure, it returns 500 INTERNAL SERVER ERROR along with a string that
    describes the related server-side error.

    """
    try:
        basename_abs = create_kernel_source_file(request.get_data())
        gen_llvm_bitcode_file(basename_abs)
        pass_output = run_oclsa_pass(basename_abs)
        kernel_features = process_pass_output(pass_output)
    except Exception as err:
        logging.error(err)
        return make_response(str(err), 500)
    else:
        return jsonify(kernel_features)
    finally:
        unlink_kernel_files(basename_abs)


def unlink_kernel_files(basename: str) -> None:
    for suffix in [".cl", ".bc"]:
        try:
            os.unlink(basename + suffix)
        except OSError as err:
            logging.debug("os.unlink(): %s", err)


def process_pass_output(output: bytes) -> Dict[str, int]:
    kernel_features = {}
    for line in output.split(b"\n"):
        tokens = list(map(lambda x: str(x, "utf-8"), line.strip().split()))
        if len(tokens) == 0:
            continue
        if tokens[0].startswith("Num"):
            try:
                kernel_features[tokens[0][3:]] += int(tokens[-1])
            except KeyError:
                kernel_features[tokens[0][3:]] = int(tokens[-1])
    return kernel_features


def run_oclsa_pass(basename: str) -> bytes:
    return subprocess.run(
            OCLSA_PASS_CMD % basename, shell=True, check=True,
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
            ).stderr


def gen_llvm_bitcode_file(basename: str) -> None:
    subprocess.run(
            CLANG_FRONTEND_CMD.format(basename), shell=True, check=True,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def create_kernel_source_file(opencl_source_code: bytes) -> str:
    tmp_fd, tmp_fname = tempfile.mkstemp(suffix=".cl", dir=TMP_PATH, text=True)
    os.write(tmp_fd, bytes(CLANG_BITCHING_KILLER, "utf-8"))
    os.write(tmp_fd, opencl_source_code)
    os.close(tmp_fd)

    return os.path.join(TMP_PATH, tmp_fname).rsplit(".", 1)[0]


if __name__ == "__main__":
    lf = "%(asctime)s %(name)-8s %(module)s %(levelname)-8s    %(message)s"
    logging.basicConfig(format=lf, level=logging.DEBUG)

    app = Flask(__name__)
    app.add_url_rule(
            "/extract",
            "extract_features",
            extract_features,
            methods=["POST"],
    )
    app.run(
            host="0.0.0.0",
            port=54242,
            threaded=True,
            debug=True,
    )
