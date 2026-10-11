# {{PROJECT_NAME}} game project (SkyEngine).
#
# Independent CMake entry: consumes the engine as a library (engine + plugin targets)
# instead of adding the engine's root project. Point SKY_ENGINE_ROOT at the SkyEngine
# repository (via a preset or the environment).
cmake_minimum_required(VERSION 3.20)

project({{PROJECT_NAME}} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_C_STANDARD 17)

if (NOT DEFINED SKY_ENGINE_ROOT OR SKY_ENGINE_ROOT STREQUAL "")
    set(SKY_ENGINE_ROOT "$ENV{SKY_ENGINE_ROOT}")
endif ()
if (SKY_ENGINE_ROOT STREQUAL "")
    message(FATAL_ERROR "SKY_ENGINE_ROOT is not set: point it at the SkyEngine repository.")
endif ()
set(ENGINE_ROOT "${SKY_ENGINE_ROOT}")

# Platform name used for the output tree layout (<Project>/build/<platform>/bin).
if (NOT DEFINED SKY_PLATFORM OR SKY_PLATFORM STREQUAL "")
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" SKY_PLATFORM)
    if (SKY_PLATFORM STREQUAL "darwin")
        set(SKY_PLATFORM "macos")
    endif ()
endif ()

# Unified runtime/library output. Android must NOT override these (gradle's
# externalNativeBuild collects the shared libraries from the CMake build tree).
if (NOT ANDROID)
    set(_sky_bin "${CMAKE_CURRENT_SOURCE_DIR}/../build/${SKY_PLATFORM}/bin")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${_sky_bin}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${_sky_bin}")
    # Keep the $<CONFIG> subdir consistent with the engine's deploy path
    # (cmake/functions.cmake copies to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>).
    foreach (_cfg Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER "${_cfg}" _cfgU)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfgU} "${_sky_bin}/${_cfg}")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_cfgU} "${_sky_bin}/${_cfg}")
    endforeach ()
endif ()

include(${ENGINE_ROOT}/cmake/functions.cmake)
include(${ENGINE_ROOT}/cmake/options.cmake)
include(${ENGINE_ROOT}/cmake/plugin_switches.cmake)
include(${ENGINE_ROOT}/cmake/configuration.cmake)
include(${ENGINE_ROOT}/cmake/thirdparty.cmake)

# Third-party: reuse the engine bootstrap output (python/third_party.py -p <platform>).

add_subdirectory("${ENGINE_ROOT}/engine"  "${CMAKE_BINARY_DIR}/engine")
add_subdirectory("${ENGINE_ROOT}/plugins" "${CMAKE_BINARY_DIR}/plugins")

# Preset-provided extra compile definitions/flags (see export_presets.json).
if (SKY_EXTRA_CXX_DEFINITIONS)
    add_compile_definitions(${SKY_EXTRA_CXX_DEFINITIONS})
endif ()
if (SKY_EXTRA_CXX_FLAGS)
    add_compile_options(${SKY_EXTRA_CXX_FLAGS})
endif ()

# Project-owned platform launcher (defines the `Launcher` target).
if (ANDROID)
    add_subdirectory(android/cpp)
elseif (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_subdirectory(ios)
elseif (APPLE)
    add_subdirectory(macos)
elseif (WIN32)
    add_subdirectory(windows)
endif ()

# Game module(s).
add_subdirectory(source)

# Bind every `DEPENDENCIES Launcher` registration (engine, plugins) to this launcher.
sky_set_dependency(TARGET Launcher)

# Deploy the engine builtin configs first, then the project configs (project wins).
# A custom target is used because `Launcher` is defined in a subdirectory.
add_custom_target({{PROJECT_NAME}}Deploy ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ENGINE_ROOT}/configs"
        "$<TARGET_FILE_DIR:Launcher>/configs"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../configs"
        "$<TARGET_FILE_DIR:Launcher>/configs"
    DEPENDS Launcher
    COMMENT "Deploying engine + project configs"
)
