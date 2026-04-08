file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

if (WIN32)
    file(GLOB_RECURSE PLATFORM_SRC platform/windows/*)
elseif(APPLE)
    file(GLOB_RECURSE PLATFORM_SRC platform/macos/*)
    find_package(Vulkan REQUIRED)
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
        3rdParty::vulkan_headers
        3rdParty::vma
)

target_compile_definitions(AuroraVulkan PUBLIC VK_NO_PROTOTYPES)

#if (SKY_BUILD_XR)
#    target_compile_definitions(AuroraVulkan PUBLIC -DXR_USE_GRAPHICS_API_VULKAN)
#endif ()

if(ANDROID)
    target_compile_definitions(AuroraVulkan PUBLIC -DVMA_VULKAN_VERSION=1002000)
endif()

sky_add_dependency(TARGET AuroraVulkan DEPENDENCIES Launcher Editor AuroraTest)
