SET(3RD_PATH "" CACHE STRING "SkyEngine 3rd path")

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