# SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
# SPDX-FileCopyrightText: © 2017, University of Cincinnati
# SPDX-License-Identifier: BSD-3-Clause
# Based on https://crascit.com/2016/04/09/using-ccache-with-cmake/

cmake_minimum_required(VERSION 3.10)

# This module is included once ZRYTHM_COMPILER_CACHE has been set to a
# non-empty value ("ccache" or "sccache"). It locates the requested compiler
# cache tool and routes compilation through it.
#
# For Ninja, Makefiles and the Visual Studio generators the native
# CMAKE_<LANG>_COMPILER_LAUNCHER mechanism is used. For the Xcode generator
# (which ignores that variable) wrapper scripts are generated, matching the
# original crascit approach.
#
# The cache *backend* (local disk, S3, redis, ...) is configured entirely
# outside of CMake, via environment variables or the tool's own config file.
# No credentials belong in this file.

find_program(COMPILER_CACHE_PROGRAM "${ZRYTHM_COMPILER_CACHE}")
if(COMPILER_CACHE_PROGRAM)
    # ccache wants CCACHE_CPP2=true; sccache does not use it.
    if(ZRYTHM_COMPILER_CACHE STREQUAL "ccache")
        set(_CACHE_ENV_LINE "export CCACHE_CPP2=true\n")
    else()
        set(_CACHE_ENV_LINE "")
    endif()

    # Xcode cannot use CMAKE_<LANG>_COMPILER_LAUNCHER, so emit wrapper scripts
    # that prepend the cache tool. Xcode also omits the compiler as the first
    # argument (unlike Ninja/Makefiles); the scripts handle both forms.
    file(WRITE "${CMAKE_BINARY_DIR}/launch-c"
        "#!/usr/bin/env sh\n"
        "\n"
        "if [ \"$1\" = \"${CMAKE_C_COMPILER}\" ] ; then\n"
        "    shift\n"
        "fi\n"
        "\n"
        "${_CACHE_ENV_LINE}"
        "exec \"${COMPILER_CACHE_PROGRAM}\" \"${CMAKE_C_COMPILER}\" \"$@\"\n"
        )

    file(WRITE "${CMAKE_BINARY_DIR}/launch-cxx"
        "#!/usr/bin/env sh\n"
        "\n"
        "if [ \"$1\" = \"${CMAKE_CXX_COMPILER}\" ] ; then\n"
        "    shift\n"
        "fi\n"
        "\n"
        "${_CACHE_ENV_LINE}"
        "exec \"${COMPILER_CACHE_PROGRAM}\" \"${CMAKE_CXX_COMPILER}\" \"$@\"\n"
        )

    file(WRITE "${CMAKE_BINARY_DIR}/launch-cuda"
        "#!/usr/bin/env sh\n"
        "\n"
        "if [ \"$1\" = \"${CMAKE_CUDA_COMPILER}\" ] ; then\n"
        "    shift\n"
        "fi\n"
        "\n"
        "${_CACHE_ENV_LINE}"
        "exec \"${COMPILER_CACHE_PROGRAM}\" \"${CMAKE_CUDA_COMPILER}\" \"$@\"\n"
        )

    execute_process(COMMAND chmod a+rx
        "${CMAKE_BINARY_DIR}/launch-c"
        "${CMAKE_BINARY_DIR}/launch-cxx"
        "${CMAKE_BINARY_DIR}/launch-cuda"
    )

    if(CMAKE_GENERATOR STREQUAL "Xcode")
        # Route compilation and linking through the wrapper scripts.
        set(CMAKE_XCODE_ATTRIBUTE_CC         "${CMAKE_BINARY_DIR}/launch-c")
        set(CMAKE_XCODE_ATTRIBUTE_CXX        "${CMAKE_BINARY_DIR}/launch-cxx")
        set(CMAKE_XCODE_ATTRIBUTE_LD         "${CMAKE_BINARY_DIR}/launch-c")
        set(CMAKE_XCODE_ATTRIBUTE_LDPLUSPLUS "${CMAKE_BINARY_DIR}/launch-cxx")
    else()
        # Ninja, Unix Makefiles, Visual Studio: use the launcher variable.
        set(CMAKE_C_COMPILER_LAUNCHER    "${COMPILER_CACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER  "${COMPILER_CACHE_PROGRAM}")
        set(CMAKE_CUDA_COMPILER_LAUNCHER "${COMPILER_CACHE_PROGRAM}")
    endif()

    message(STATUS "Using compiler cache: ${ZRYTHM_COMPILER_CACHE} (${COMPILER_CACHE_PROGRAM})")
else()
    message(STATUS "Not using compiler cache (${ZRYTHM_COMPILER_CACHE} not found)")
endif()
