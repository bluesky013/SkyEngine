SET(3RD_PATH "" CACHE STRING "SkyEngine 3rd path")

option(SKY_BUILD_EDITOR "build editor" OFF)
option(SKY_BUILD_GLES "build gles" OFF)
option(SKY_BUILD_TEST "build test" OFF)
option(SKY_BUILD_DXC "build dxcompiler" OFF)
option(SKY_EDITOR "editor mode" OFF)
option(SKY_USE_TRACY "use tracy profiler" OFF)

# todo: controlled by project config json.
option(SKY_BUILD_XR "xr plugin" OFF)