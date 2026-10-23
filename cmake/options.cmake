SET(3RD_PATH "" CACHE STRING "SkyEngine 3rd path")

if (3RD_PATH STREQUAL "")
    # Load the third-party cache for the current build platform (per-platform
    # files avoid one platform clobbering another's 3RD_PATH). Falls back to the
    # legacy single file if no per-platform cache exists.
    if (WIN32)
        set(_sky_tp_platform "Win32")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
        set(_sky_tp_platform "Android")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(_sky_tp_platform "IOS")
    elseif (APPLE)
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            set(_sky_tp_platform "MacOS-arm")
        else ()
            set(_sky_tp_platform "MacOS-x86")
        endif ()
    elseif (UNIX)
        set(_sky_tp_platform "Linux")
    endif ()

    set(_sky_tp_cache "${ENGINE_ROOT}/build_3rd/thirdparty_cache_${_sky_tp_platform}.cmake")
    if (_sky_tp_platform AND EXISTS "${_sky_tp_cache}")
        include("${_sky_tp_cache}")
    elseif (EXISTS "${ENGINE_ROOT}/build_3rd/thirdparty_cache.cmake")
        include("${ENGINE_ROOT}/build_3rd/thirdparty_cache.cmake")
    endif ()
endif ()

if (NOT 3RD_PATH STREQUAL "")
    set(3RD_PATH "${3RD_PATH}" CACHE PATH "SkyEngine 3rd path" FORCE)
endif ()

option(SKY_BUILD_TOOL "build tools/ code only" OFF)
option(SKY_BUILD_SANDBOX "build the sandbox editor" OFF)
option(SKY_DEVELOP "enable developer facilities, independent of build configuration" OFF)

if (SKY_BUILD_SANDBOX OR SKY_BUILD_TOOL)
    set(SKY_DEVELOP ON)
endif ()

option(SKY_PYTHON_SSL  "python ssl/_hashlib via static OpenSSL" OFF)
option(SKY_BUILD_TEST  "build test"           OFF)
option(SKY_USE_TRACY   "use tracy profiler"   OFF)
option(SKY_MATH_SIMD   "enable simd math"     OFF)
# Disable FP contraction (a*b+c -> fma) and force IEEE-conformant FP so results
# match across compilers/architectures (required for lockstep / deterministic
# physics). Off by default; enable for deterministic/replay builds.
option(SKY_DETERMINISTIC_FP "deterministic floating point (no FMA contraction)" OFF)
option(SKY_ANIMATION_ACL "enable acl2 animation compression" ON)

# FreeType UI font adaptor (engine/ui/freetype + sandbox). Decoupled from the
# removed legacy freetype render plugin; on by default.
option(SKY_BUILD_FREETYPE "enable the FreeType UI font provider" ON)

# Application identity for the launcher bundle. Overridden per project by the
# generated IDE project/CMake preset so applicationId / CFBundleIdentifier are
# driven by project settings rather than hardcoded.
set(SKY_BUNDLE_ID "com.skyengine.launcher" CACHE STRING "Launcher bundle/application identifier")
set(SKY_APPLE_TEAM_ID "" CACHE STRING "Apple DEVELOPMENT_TEAM for code signing")
set(SKY_APPLE_SIGNING_IDENTITY "" CACHE STRING "Apple code signing identity")
set(SKY_APPLE_PROVISIONING_PROFILE "" CACHE STRING "Apple provisioning profile")

# Runtime module packaging: `dynamic` (one shared library per module, dlopen) or
# `static` (modules compiled into the host + a registration cpp generated from the
# plugin config). Default dynamic = no behavior change. See the
# `add-static-module-bundling` change.
set(SKY_MODULE_PACKAGING "dynamic" CACHE STRING "Runtime module packaging: dynamic | static")

# Static packaging defines SKY_MODULE_STATIC process-wide so the framework's
# registration hook and every module's tagged factory compile consistently.
if (SKY_MODULE_PACKAGING STREQUAL "static")
    add_compile_definitions(SKY_MODULE_STATIC)
endif()
