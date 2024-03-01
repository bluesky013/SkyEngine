file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    file(GLOB_RECURSE PLATFORM_SRC platform/windows/*)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    file(GLOB_RECURSE PLATFORM_SRC platform/android/*)
endif ()

file (GLOB_RECURSE XR_RHI_VK_SRC rhi/vulkan/*)

sky_add_library(TARGET XRModule.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        ${PLATFORM_SRC}
        ${XR_RHI_VK_SRC}
    PRIVATE_INC
        src
        rhi
        platform
    PUBLIC_INC
        include
    LINK_LIBS
        Framework
        SkyRenderCore
        3rdParty::OpenXR
)

sky_add_library(TARGET XRModule SHARED
    SOURCES
        XRModule.cpp
    PRIVATE_INC
    PUBLIC_INC
    LINK_LIBS
        XRModule.Static
)

sky_add_dependency(TARGET XRModule DEPENDENCIES Launcher)