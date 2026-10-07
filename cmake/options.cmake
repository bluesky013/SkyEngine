SET(3RD_PATH "" CACHE STRING "SkyEngine 3rd path")

if (3RD_PATH STREQUAL "" AND EXISTS "${ENGINE_ROOT}/build_3rd/thirdparty_cache.cmake")
    include("${ENGINE_ROOT}/build_3rd/thirdparty_cache.cmake")
endif ()

if (NOT 3RD_PATH STREQUAL "")
    set(3RD_PATH "${3RD_PATH}" CACHE PATH "SkyEngine 3rd path" FORCE)
endif ()

# SKY_BUILD_EDITOR is deprecated: the editor is built through SKY_BUILD_SANDBOX.
option(SKY_BUILD_EDITOR "build legacy editor (deprecated)" OFF)
option(SKY_BUILD_TOOL "build tools/ code only" OFF)
option(SKY_BUILD_SANDBOX "build the sandbox editor" OFF)
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
