#!/bin/sh
# Cross-compile eMP-gba for T113-S3 (eMP-toolchain).
# Usage: T113_SDK=/path/to/eMP-toolchain ./build.sh
set -e

SDK="${T113_SDK:-/home/hugokkl/eMP-t113-toolchain}"

# OpenWrt toolchain wrapper requires STAGING_DIR to set --sysroot
export STAGING_DIR="${SDK}/sysroot"

cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=cmake/build_for_t113s3.cmake \
    -DT113_SDK="${SDK}"

cmake --build build -j"$(nproc)"
