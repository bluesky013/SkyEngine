cmake_minimum_required(VERSION 3.20)
project(cpython_android C)

# Cross-builds the CPython core for Android using the NDK toolchain. `pyconfig.h`, `Modules/config.c`, and the
# object list (`libsrc.txt`) are produced beforehand by CPython's `configure` (see python/build_cpython.py).

set(CPYTHON_SRC "" CACHE PATH "CPython source tree")
set(CPYTHON_BUILD "" CACHE PATH "CPython out-of-tree configure build directory")
set(CPYTHON_FROZEN "" CACHE PATH "Directory containing the generated Python/frozen_modules headers")
set(CPYTHON_VERSION "3.13" CACHE STRING "CPython major.minor")
set(CPYTHON_PREFIX "/usr/local" CACHE STRING "Compiled-in prefix")

file(STRINGS "${CPYTHON_BUILD}/libsrc.txt" CPYTHON_LIB_SRCS)

set(CPYTHON_RESOLVED_SRCS "")
foreach(_rel ${CPYTHON_LIB_SRCS})
    if (EXISTS "${CPYTHON_BUILD}/${_rel}")
        list(APPEND CPYTHON_RESOLVED_SRCS "${CPYTHON_BUILD}/${_rel}")
    else ()
        list(APPEND CPYTHON_RESOLVED_SRCS "${CPYTHON_SRC}/${_rel}")
    endif ()
endforeach()

add_library(python_static STATIC ${CPYTHON_RESOLVED_SRCS})
set_target_properties(python_static PROPERTIES OUTPUT_NAME "python${CPYTHON_VERSION}")

target_include_directories(python_static PRIVATE
    "${CPYTHON_SRC}/Include"
    "${CPYTHON_SRC}/Include/internal"
    "${CPYTHON_SRC}/Include/internal/mimalloc"
    "${CPYTHON_SRC}/Modules/_decimal/libmpdec"
    "${CPYTHON_BUILD}"
    "${CPYTHON_FROZEN}"
    "${CPYTHON_FROZEN}/Python")

target_compile_definitions(python_static PRIVATE
    Py_BUILD_CORE
    Py_NO_ENABLE_SHARED
    "PREFIX=\"${CPYTHON_PREFIX}\""
    "EXEC_PREFIX=\"${CPYTHON_PREFIX}\""
    "VERSION=\"${CPYTHON_VERSION}\""
    "VPATH=\"${CPYTHON_BUILD}\""
    "PLATLIBDIR=\"lib\""
    CONFIG_64=1
    ANSI=1
    HAVE_UINT128_T=1)

target_compile_options(python_static PRIVATE -Wno-deprecated-declarations -Wno-unused-parameter)
