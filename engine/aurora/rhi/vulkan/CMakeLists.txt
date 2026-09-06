file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

if (WIN32)
    file(GLOB_RECURSE PLATFORM_SRC platform/windows/*)
elseif(APPLE)
    file(GLOB_RECURSE PLATFORM_SRC platform/macos/*)
    find_package(Vulkan REQUIRED)

    add_library(MoltenVk INTERFACE IMPORTED GLOBAL)
    target_link_libraries(MoltenVk INTERFACE ${Vulkan_LIBRARIES})
    target_include_directories(MoltenVk INTERFACE ${Vulkan_INCLUDE_DIRS})

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
        ${MoltenVk}
)

target_compile_definitions(AuroraVulkan PUBLIC VK_NO_PROTOTYPES)

if (WIN32)
    target_compile_definitions(AuroraVulkan PUBLIC VK_USE_PLATFORM_WIN32_KHR)
elseif (APPLE)
    target_compile_definitions(AuroraVulkan PUBLIC VK_USE_PLATFORM_METAL_EXT)
    target_compile_definitions(AuroraVulkan PRIVATE SKY_VULKAN_LIBRARY_PATH="${Vulkan_LIBRARY}")
elseif (ANDROID)
    target_compile_definitions(AuroraVulkan PUBLIC VK_USE_PLATFORM_ANDROID_KHR)
endif()

#if (SKY_BUILD_XR)
#    target_compile_definitions(AuroraVulkan PUBLIC -DXR_USE_GRAPHICS_API_VULKAN)
#endif ()

if(ANDROID)
    target_compile_definitions(AuroraVulkan PUBLIC -DVMA_VULKAN_VERSION=1002000)
endif()

sky_add_dependency(TARGET AuroraVulkan DEPENDENCIES Launcher Editor AuroraTest)
