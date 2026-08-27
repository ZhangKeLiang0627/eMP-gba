#
# cross compile env define (Allwinner T113-S3)
# toolchain: eMP-toolchain  (GCC 6.4.1 musl, arm-openwrt-linux-muslgnueabi)
#
# Usage:
#   export T113_SDK=/path/to/eMP-toolchain
#   export STAGING_DIR=$T113_SDK/sysroot
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/build_for_t113s3.cmake -DT113_SDK=$T113_SDK ..
#

SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)

SET(CMAKE_SYSTEM_PROCESSOR "arm")
SET(CMAKE_HOST_SYSTEM_PROCESSOR "arm")

if(NOT DEFINED T113_SDK)
    set(T113_SDK $ENV{T113_SDK})
endif()
if(NOT T113_SDK)
    # VM default location
    set(T113_SDK /home/hugokkl/eMP-t113-toolchain)
endif()

set(TOOLCHAIN_DIR "${T113_SDK}/toolchain/bin/")
set(SYSROOT_DIR  "${T113_SDK}/sysroot")

message(STATUS "T113 toolchain dir : ${TOOLCHAIN_DIR}")
message(STATUS "T113 sysroot dir   : ${SYSROOT_DIR}")

include_directories(
    ${SYSROOT_DIR}/usr/include
    ${SYSROOT_DIR}/usr/include/allwinner
    ${SYSROOT_DIR}/usr/include/allwinner/include
)

set(CMAKE_PREFIX_PATH /usr)

link_directories(
    ${SYSROOT_DIR}/lib
    ${SYSROOT_DIR}/usr/lib
)

add_compile_options(
    -pipe
    -march=armv7-a
    -mtune=cortex-a7
    -mfpu=neon
    -mfloat-abi=hard
    -fstack-protector
)

SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

SET(CMAKE_C_COMPILER   ${TOOLCHAIN_DIR}arm-openwrt-linux-gcc)
SET(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}arm-openwrt-linux-g++)
