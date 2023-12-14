#!/usr/bin/env bash
#
# Copyright (c) 2019-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export CONTAINER_NAME=ci_native_nowallet_libbitcoinkernel
export CI_IMAGE_NAME_TAG="docker.io/debian:bullseye"
# Use minimum supported python3.9 and clang-13, see doc/dependencies.md
export PACKAGES="python3-zmq clang-13 llvm-13 libc++abi-13-dev libc++-13-dev"
export DEP_OPTS="NO_WALLET=1 CC=clang-13 CXX='clang++-13 -stdlib=libc++'"
export GOAL="install"
export BITCOIN_CONFIG="--enable-reduce-exports --enable-experimental-util-chainstate --with-experimental-kernel-lib --enable-shared"
