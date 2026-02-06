#!/usr/bin/env python3
# Copyright (c) The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

import argparse
import os
import shlex
import subprocess
import sys


def run(cmd, **kwargs):
    print("+ " + shlex.join(cmd), flush=True)
    kwargs.setdefault("check", True)
    try:
        return subprocess.run(cmd, **kwargs)
    except Exception as e:
        sys.exit(str(e))


GENERATE_OPTIONS = {
    "standard": [
        "-DBUILD_BENCH=ON",
        "-DBUILD_KERNEL_LIB=ON",
        "-DBUILD_UTIL_CHAINSTATE=ON",
        "-DCMAKE_COMPILE_WARNING_AS_ERROR=ON",
    ],
    "fuzz": [
        "-DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON",
        "-DVCPKG_MANIFEST_FEATURES=wallet",
        "-DBUILD_GUI=OFF",
        "-DWITH_ZMQ=OFF",
        "-DBUILD_FOR_FUZZING=ON",
        "-DCMAKE_COMPILE_WARNING_AS_ERROR=ON",
    ],
}


def generate(ci_type):
    toolchain_file = os.path.join(
        os.environ["VCPKG_INSTALLATION_ROOT"],
        "scripts",
        "buildsystems",
        "vcpkg.cmake",
    )
    command = [
        "cmake",
        "-B",
        "build",
        "-Werror=dev",
        "--preset",
        "vs2022",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}",
    ] + GENERATE_OPTIONS[ci_type]
    run(command)


def build():
    command = [
        "cmake",
        "--build",
        "build",
        "--config",
        "Release",
    ]
    if run(command + ["-j", str(os.process_cpu_count())], check=False).returncode != 0:
        print("Build failure. Verbose build follows.")
        run(command + ["-j1", "--verbose"])


def prepare_tests(ci_type):
    if ci_type == "standard":
        run([sys.executable, "-m", "pip", "install", "pyzmq"])
    elif ci_type == "fuzz":
        repo_dir = os.path.join(os.environ["RUNNER_TEMP"], "qa-assets")
        clone_cmd = [
            "git",
            "clone",
            "--depth=1",
            "https://github.com/bitcoin-core/qa-assets",
            repo_dir,
        ]
        run(clone_cmd)
        print("Using qa-assets repo from commit ...")
        run(["git", "-C", repo_dir, "log", "-1"])


def main():
    parser = argparse.ArgumentParser(description="Utility to run Windows CI steps.")
    parser.add_argument("ci_type", choices=GENERATE_OPTIONS, help="CI type to run.")
    steps = [
        "generate",
        "build",
        "prepare_tests",
    ]
    parser.add_argument("step", choices=steps, help="CI step to perform.")
    args = parser.parse_args()

    if args.step == "generate":
        generate(args.ci_type)
    elif args.step == "build":
        build()
    elif args.step == "prepare_tests":
        prepare_tests(args.ci_type)


if __name__ == "__main__":
    main()
