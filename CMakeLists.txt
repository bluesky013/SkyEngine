cmake_minimum_required(VERSION 3.19)  # 3.19+ required for string(JSON ...)

# ---------------------------------------------------------------------------
# Third-party dependency strategy (must be configured before project()).
#   LOCAL    : resolve deps via vcpkg (download/build to local workspace dirs)
#   PREBUILT : resolve deps from prebuilt 3RD_PATH tree
# ---------------------------------------------------------------------------
set(SKY_THIRDPARTY_MODE "LOCAL" CACHE STRING "Third-party mode: LOCAL or PREBUILT")
set_property(CACHE SKY_THIRDPARTY_MODE PROPERTY STRINGS LOCAL PREBUILT)
option(SKY_THIRDPARTY_USE_PREBUILT "Prefer reusable prebuilt binaries from local/CI cache" ON)
set(SKY_THIRDPARTY_ROOT "${CMAKE_SOURCE_DIR}/.thirdparty" CACHE PATH "Local third-party root for downloads/installed/cache")

if (NOT SKY_THIRDPARTY_MODE STREQUAL "LOCAL" AND NOT SKY_THIRDPARTY_MODE STREQUAL "PREBUILT")
    message(FATAL_ERROR "Invalid SKY_THIRDPARTY_MODE='${SKY_THIRDPARTY_MODE}'. Expected LOCAL or PREBUILT.")
endif()

if (SKY_THIRDPARTY_MODE STREQUAL "LOCAL")
    # vcpkg bootstrap + manifest mode. Everything is resolved inside workspace-local dirs.
    set(_sky_thirdparty_downloads "${SKY_THIRDPARTY_ROOT}/downloads")
    set(_sky_thirdparty_installed "${SKY_THIRDPARTY_ROOT}/installed")
    set(_sky_thirdparty_binary_cache "${SKY_THIRDPARTY_ROOT}/binary-cache")

    file(MAKE_DIRECTORY "${SKY_THIRDPARTY_ROOT}")
    file(MAKE_DIRECTORY "${_sky_thirdparty_downloads}")
    file(MAKE_DIRECTORY "${_sky_thirdparty_installed}")
    file(MAKE_DIRECTORY "${_sky_thirdparty_binary_cache}")

    include(${CMAKE_SOURCE_DIR}/cmake/vcpkg_bootstrap.cmake)
    x_vcpkg_bootstrap()
    set(VCPKG_BOOTSTRAP_OPTIONS "-disableMetrics")
    set(VCPKG_INSTALL_OPTIONS "--no-print-usage")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file" FORCE)
    list(APPEND VCPKG_FEATURE_FLAGS manifests)

    set(VCPKG_DOWNLOADS "${_sky_thirdparty_downloads}" CACHE PATH "vcpkg downloads dir" FORCE)
    set(VCPKG_INSTALLED_DIR "${_sky_thirdparty_installed}" CACHE PATH "vcpkg installed dir" FORCE)

    if (SKY_THIRDPARTY_USE_PREBUILT)
        set(VCPKG_BINARY_SOURCES "clear;files,${_sky_thirdparty_binary_cache},readwrite" CACHE STRING "vcpkg binary sources" FORCE)
    else()
        # Keep cache local even in source-build preference mode.
        set(VCPKG_BINARY_SOURCES "clear;files,${_sky_thirdparty_binary_cache},readwrite" CACHE STRING "vcpkg binary sources" FORCE)
    endif()

    set(ENV{VCPKG_DEFAULT_BINARY_CACHE} "${_sky_thirdparty_binary_cache}")
    set(SKY_USE_VCPKG ON CACHE BOOL "use vcpkg for third-party dependencies" FORCE)
    unset(_sky_thirdparty_downloads)
    unset(_sky_thirdparty_installed)
    unset(_sky_thirdparty_binary_cache)
else()
    set(SKY_USE_VCPKG OFF CACHE BOOL "use vcpkg for third-party dependencies" FORCE)
endif()

# detect platform
# TODO: try to adapt to more platforms
if(WIN32)
    set(VCPKG_TARGET_TRIPLET x64-windows)
elseif(ANDROID)
    set(VCPKG_TARGET_TRIPLET arm64-android)
elseif(IOS)
    set(VCPKG_TARGET_TRIPLET arm64-ios)
elseif(APPLE)
    set(VCPKG_TARGET_TRIPLET arm64-osx)
elseif(UNIX)
    set(VCPKG_TARGET_TRIPLET x64-linux)
endif()

# ---------------------------------------------------------------------------
# vcpkg manifest-mode: map SKY_BUILD_* options to VCPKG_MANIFEST_FEATURES
# so that vcpkg installs only the optional packages the user asked for.
# These cache variables mirror the options defined later in cmake/options.cmake
# but must be evaluated before project() because vcpkg runs at that point.
# ---------------------------------------------------------------------------
set(_sky_vcpkg_detected FALSE)
if (SKY_THIRDPARTY_MODE STREQUAL "LOCAL")
    set(_sky_vcpkg_detected TRUE)
elseif (CMAKE_TOOLCHAIN_FILE)
    string(FIND "${CMAKE_TOOLCHAIN_FILE}" "vcpkg" _vcpkg_idx)
    if (NOT _vcpkg_idx EQUAL -1)
        set(_sky_vcpkg_detected TRUE)
    endif()
    unset(_vcpkg_idx)
endif()

if (_sky_vcpkg_detected)
    # Point vcpkg at our custom triplets (Android, iOS, etc.)
    set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/triplets" CACHE STRING "" FORCE)

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
