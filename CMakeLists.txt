cmake_minimum_required(VERSION 3.19)  # 3.19+ required for string(JSON ...)

# ---------------------------------------------------------------------------
# vcpkg manifest-mode: map SKY_BUILD_* options to VCPKG_MANIFEST_FEATURES
# so that vcpkg installs only the optional packages the user asked for.
# These cache variables mirror the options defined later in cmake/options.cmake
# but must be evaluated before project() because vcpkg runs at that point.
# ---------------------------------------------------------------------------
set(_sky_vcpkg_detected FALSE)
if (SKY_USE_VCPKG)
    set(_sky_vcpkg_detected TRUE)
elseif (CMAKE_TOOLCHAIN_FILE)
    string(FIND "${CMAKE_TOOLCHAIN_FILE}" "vcpkg" _vcpkg_idx)
    if (NOT _vcpkg_idx EQUAL -1)
        set(_sky_vcpkg_detected TRUE)
    endif()
    unset(_vcpkg_idx)
endif()

if (_sky_vcpkg_detected)
    set(_sky_vcpkg_features "")

    option(SKY_BUILD_EDITOR      "build editor"                OFF)
    option(SKY_BUILD_BULLET      "build bullet physics plugin"  OFF)
    option(SKY_BUILD_RECAST      "build recast navigation"      OFF)
    option(SKY_USE_TRACY         "use tracy profiler"            OFF)
    option(SKY_BUILD_FREETYPE    "build freetype text plugin"    OFF)
    option(SKY_BUILD_COMPRESSION "build lz4 compression plugin"  OFF)

    if (SKY_BUILD_EDITOR)
        list(APPEND _sky_vcpkg_features "editor")
    endif()
    if (SKY_BUILD_BULLET)
        list(APPEND _sky_vcpkg_features "bullet")
    endif()
    if (SKY_BUILD_RECAST)
        list(APPEND _sky_vcpkg_features "recast")
    endif()
    if (SKY_USE_TRACY)
        list(APPEND _sky_vcpkg_features "tracy")
    endif()
    if (SKY_BUILD_FREETYPE)
        list(APPEND _sky_vcpkg_features "freetype")
    endif()
    if (SKY_BUILD_COMPRESSION)
        list(APPEND _sky_vcpkg_features "compression")
    endif()

    if (_sky_vcpkg_features)
        list(JOIN _sky_vcpkg_features ";" _sky_vcpkg_features_str)
        set(VCPKG_MANIFEST_FEATURES "${_sky_vcpkg_features_str}" CACHE STRING "" FORCE)
    endif()
    unset(_sky_vcpkg_features)
endif()
unset(_sky_vcpkg_detected)

PROJECT(SkyEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(ENGINE_ROOT ${CMAKE_CURRENT_LIST_DIR})

include(${ENGINE_ROOT}/cmake/functions.cmake)
include(${ENGINE_ROOT}/cmake/options.cmake)
include(${ENGINE_ROOT}/cmake/plugin_switches.cmake)
include(${ENGINE_ROOT}/cmake/configuration.cmake)
include(${ENGINE_ROOT}/cmake/thirdparty.cmake)

if (NOT ANDROID)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ENGINE_ROOT}/output/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ENGINE_ROOT}/output/bin)
endif()

if (SKY_BUILD_TEST)
    enable_testing()
endif()

add_subdirectory(engine)
add_subdirectory(plugins)

if (SKY_BUILD_TOOL)
    add_subdirectory(tools)
endif()


sky_set_dependency(TARGET Launcher)
if (SKY_BUILD_EDITOR)
    sky_set_dependency(TARGET Editor)
endif()
