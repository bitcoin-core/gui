#!/usr/bin/env python3
# Copyright (c) The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

import argparse
import shlex
import subprocess
import sys
from pathlib import Path


def run(cmd, **kwargs):
    print("+ " + shlex.join(cmd), flush=True)
    kwargs.setdefault("check", True)
    try:
        return subprocess.run(cmd, **kwargs)
    except Exception as e:
        sys.exit(str(e))


def print_version():
    bitcoind = Path.cwd() / "bin" / "bitcoind.exe"
    run([str(bitcoind), "-version"])


def main():
    parser = argparse.ArgumentParser(description="Utility to run Windows CI steps.")
    steps = [
        "print_version",
    ]
    parser.add_argument("step", choices=steps, help="CI step to perform.")
    args = parser.parse_args()

    if args.step == "print_version":
        print_version()


if __name__ == "__main__":
    main()
