SET(3RD_PATH "" CACHE STRING "SkyEngine 3rd path")

if (3RD_PATH STREQUAL "" AND EXISTS "${ENGINE_ROOT}/build_3rd/thirdparty_cache.cmake")
    include("${ENGINE_ROOT}/build_3rd/thirdparty_cache.cmake")
endif ()

if (NOT 3RD_PATH STREQUAL "")
    set(3RD_PATH "${3RD_PATH}" CACHE PATH "SkyEngine 3rd path" FORCE)
endif ()

option(SKY_BUILD_EDITOR "build editor" OFF)
option(SKY_EDITOR "editor mode" OFF)

if (SKY_BUILD_EDITOR)
    set(SKY_EDITOR ON)
endif ()

option(SKY_BUILD_GLES  "build gles"          OFF)
option(SKY_BUILD_TEST  "build test"           OFF)
option(SKY_USE_TRACY   "use tracy profiler"   OFF)
option(SKY_BUILD_TOOL  "build tools"          OFF)
option(SKY_MATH_SIMD   "enable simd math"     OFF)
