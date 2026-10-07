# {{PROJECT_NAME}} game project (SkyEngine).
#
# Build entry for the project's game module. The engine is consumed from its
# source tree; point SKY_ENGINE_ROOT at the SkyEngine repository (via this file,
# a CMake preset, or the environment).
cmake_minimum_required(VERSION 3.20)

project({{PROJECT_NAME}} LANGUAGES C CXX)

if (NOT DEFINED SKY_ENGINE_ROOT OR SKY_ENGINE_ROOT STREQUAL "")
    set(SKY_ENGINE_ROOT "$ENV{SKY_ENGINE_ROOT}")
endif ()
if (SKY_ENGINE_ROOT STREQUAL "")
    message(FATAL_ERROR "SKY_ENGINE_ROOT is not set: point it at the SkyEngine repository (e.g. -DSKY_ENGINE_ROOT=/path/to/SkyEngine).")
endif ()

# Third-party: reuse the engine bootstrap output (python/third_party.py -p <platform>).
# 3RD_PATH may be provided here or through build_3rd/thirdparty_cache.cmake.

add_subdirectory("${SKY_ENGINE_ROOT}" "${CMAKE_BINARY_DIR}/engine")
add_subdirectory(source)
