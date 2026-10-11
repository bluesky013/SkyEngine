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

# SKY_BUILD_EDITOR is deprecated: the editor is built through SKY_BUILD_SANDBOX.
option(SKY_BUILD_EDITOR "build legacy editor (deprecated)" OFF)
option(SKY_BUILD_TOOL "build tools/ code only" OFF)
option(SKY_BUILD_SANDBOX "build the sandbox editor" OFF)
# Legacy (pre-aurora) shader/render stack. Deprecated; not used by the aurora
# runtime or the sandbox editor. Off by default so a full build does not need it.
option(SKY_BUILD_LEGACY_RENDER "build the legacy (pre-aurora) shader/render stack" OFF)
option(SKY_DEVELOP "develop mode" OFF)

if (SKY_BUILD_SANDBOX OR SKY_BUILD_EDITOR OR SKY_BUILD_TOOL)
    set(SKY_DEVELOP ON)
endif ()

option(SKY_BUILD_GLES  "build gles"          OFF)
option(SKY_PYTHON_SSL  "python ssl/_hashlib via static OpenSSL" OFF)
option(SKY_BUILD_TEST  "build test"           OFF)
option(SKY_USE_TRACY   "use tracy profiler"   OFF)
option(SKY_MATH_SIMD   "enable simd math"     OFF)
# Disable FP contraction (a*b+c -> fma) and force IEEE-conformant FP so results
# match across compilers/architectures (required for lockstep / deterministic
# physics). Off by default; enable for deterministic/replay builds.
option(SKY_DETERMINISTIC_FP "deterministic floating point (no FMA contraction)" OFF)
option(SKY_ANIMATION_ACL "enable acl2 animation compression" ON)

# FreeType UI font provider (engine/ui/core + sandbox). Decoupled from the
# removed legacy freetype render plugin; on by default.
option(SKY_BUILD_FREETYPE "enable the FreeType UI font provider" ON)
