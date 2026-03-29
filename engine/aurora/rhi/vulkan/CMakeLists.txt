file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

find_package(Vulkan)
if (NOT Vulkan_FOUND)
    message(WARNING "Vulkan SDK not found")
endif()

if (WIN32)
    file(GLOB_RECURSE PLATFORM_SRC platform/windows/*)
elseif(APPLE)
    file(GLOB_RECURSE PLATFORM_SRC platform/macos/*)
elseif(ANDROID)
    file(GLOB_RECURSE PLATFORM_SRC platform/android/*)
endif()

sky_add_library(TARGET AuroraVulkan SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        ${PLATFORM_SRC}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        Aurora.RHI
        Vulkan::Vulkan
)

#if (SKY_BUILD_XR)
#    target_compile_definitions(AuroraVulkan PUBLIC -DXR_USE_GRAPHICS_API_VULKAN)
#endif ()
#
#if(ANDROID)
#    target_compile_definitions(AuroraVulkan PUBLIC -DVMA_VULKAN_VERSION=1002000)
#endif()

sky_add_dependency(TARGET AuroraVulkan DEPENDENCIES Launcher Editor AuroraTest)
