#!/usr/bin/env bash
#
# Copyright (c) 2019-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export CI_IMAGE_NAME_TAG="docker.io/ubuntu:24.04"
# Only install BCC tracing packages in Cirrus CI.
if [[ "${CIRRUS_CI}" == "true" ]]; then
  BPFCC_PACKAGE="bpfcc-tools linux-headers-$(uname --kernel-release)"
  export CI_CONTAINER_CAP="--privileged -v /sys/kernel:/sys/kernel:rw"
else
  BPFCC_PACKAGE=""
  export CI_CONTAINER_CAP="--cap-add SYS_PTRACE"  # If run with (ASan + LSan), the container needs access to ptrace (https://github.com/google/sanitizers/issues/764)
fi

export CONTAINER_NAME=ci_native_asan
export PACKAGES="systemtap-sdt-dev clang-17 llvm-17 libclang-rt-17-dev python3-zmq qtbase5-dev qttools5-dev-tools libevent-dev libboost-dev libdb5.3++-dev libminiupnpc-dev libnatpmp-dev libzmq3-dev libqrencode-dev libsqlite3-dev ${BPFCC_PACKAGE}"
export NO_DEPENDS=1
export GOAL="install"
export BITCOIN_CONFIG="--enable-c++20 --enable-usdt --enable-zmq --with-incompatible-bdb --with-gui=qt5 \
CPPFLAGS='-DARENA_DEBUG -DDEBUG_LOCKORDER' \
--with-sanitizers=address,float-divide-by-zero,integer,undefined \
CC='clang-17 -ftrivial-auto-var-init=pattern' CXX='clang++-17 -ftrivial-auto-var-init=pattern'"
