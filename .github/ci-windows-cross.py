#!/usr/bin/env python3
# Copyright (c) The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

import argparse
import os
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


def check_manifests():
    release_dir = Path.cwd() / "bin"
    manifest_path = release_dir / "bitcoind.manifest"

    cmd_bitcoind_manifest = [
        "mt.exe",
        "-nologo",
        f"-inputresource:{release_dir / 'bitcoind.exe'}",
        f"-out:{manifest_path}",
    ]
    run(cmd_bitcoind_manifest)
    print(manifest_path.read_text())

    skipped = {  # Skip as they currently do not have manifests
        "fuzz.exe",
        "bench_bitcoin.exe",
        "test_kernel.exe",
    }
    for entry in release_dir.iterdir():
        if entry.suffix.lower() != ".exe":
            continue
        if entry.name in skipped:
            print(f"Skipping {entry.name} (no manifest present)")
            continue
        print(f"Checking {entry.name}")
        run(["mt.exe", "-nologo", f"-inputresource:{entry}", "-validate_manifest"])


def prepare_tests():
    workspace = Path.cwd()
    config_path = workspace / "test" / "config.ini"
    rpcauth_path = workspace / "share" / "rpcauth" / "rpcauth.py"
    replacements = {
        "SRCDIR=": f"SRCDIR={workspace}",
        "BUILDDIR=": f"BUILDDIR={workspace}",
        "RPCAUTH=": f"RPCAUTH={rpcauth_path}",
    }
    lines = config_path.read_text().splitlines()
    for index, line in enumerate(lines):
        for prefix, new_value in replacements.items():
            if line.startswith(prefix):
                lines[index] = new_value
                break
    content = "\n".join(lines) + "\n"
    config_path.write_text(content)
    print(content)
    previous_releases_dir = Path(os.environ["PREVIOUS_RELEASES_DIR"])
    cmd_download_prev_rel = [
        sys.executable,
        str(workspace / "test" / "get_previous_releases.py"),
        "--target-dir",
        str(previous_releases_dir),
    ]
    run(cmd_download_prev_rel)
    run([sys.executable, "-m", "pip", "install", "pyzmq"])


def main():
    parser = argparse.ArgumentParser(description="Utility to run Windows CI steps.")
    steps = [
        "print_version",
        "check_manifests",
        "prepare_tests",
    ]
    parser.add_argument("step", choices=steps, help="CI step to perform.")
    args = parser.parse_args()

    if args.step == "print_version":
        print_version()
    elif args.step == "check_manifests":
        check_manifests()
    elif args.step == "prepare_tests":
        prepare_tests()


if __name__ == "__main__":
    main()
