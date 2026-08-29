# M2 只引用本地已 relocation 的 Buildroot SDK，不把工具链路径写死进仓库。
if(NOT DEFINED ENV{IMX6ULL_SDK_ROOT} OR "$ENV{IMX6ULL_SDK_ROOT}" STREQUAL "")
    message(FATAL_ERROR
            "Set IMX6ULL_SDK_ROOT to the relocated Buildroot SDK root")
endif()

file(TO_CMAKE_PATH "$ENV{IMX6ULL_SDK_ROOT}" IMX6ULL_SDK_ROOT)
set(IMX6ULL_TARGET_TRIPLE arm-buildroot-linux-gnueabihf)
set(IMX6ULL_SYSROOT
    "${IMX6ULL_SDK_ROOT}/${IMX6ULL_TARGET_TRIPLE}/sysroot")
set(IMX6ULL_C_COMPILER
    "${IMX6ULL_SDK_ROOT}/bin/${IMX6ULL_TARGET_TRIPLE}-gcc")

if(NOT EXISTS "${IMX6ULL_C_COMPILER}")
    message(FATAL_ERROR "Cross compiler not found: ${IMX6ULL_C_COMPILER}")
endif()
if(NOT EXISTS "${IMX6ULL_SYSROOT}/usr/include/stdio.h")
    message(FATAL_ERROR "Buildroot sysroot is incomplete: ${IMX6ULL_SYSROOT}")
endif()

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7)
set(CMAKE_C_COMPILER "${IMX6ULL_C_COMPILER}")
set(CMAKE_SYSROOT "${IMX6ULL_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "${IMX6ULL_SYSROOT}")

# 程序在 Ubuntu 主机执行，头文件/库/包只能从目标 sysroot 查找。
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
