file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

find_package(Vulkan)
if (NOT Vulkan_FOUND)
    message("vulkan sdk not found")
endif()

add_library(VulkanSdk INTERFACE IMPORTED GLOBAL
    ../../tool/image/main.cpp
    ../../tool/image/include/TextureCompressor.h
    ../../tool/image/include/ImageTool.h
    ../../tool/image/src/ImageTool.cpp)
target_link_libraries(VulkanSdk INTERFACE ${Vulkan_LIBRARIES})
target_include_directories(VulkanSdk INTERFACE ${Vulkan_INCLUDE_DIRS})

if (WIN32)
    file(GLOB_RECURSE PLATFORM_SRC platform/windows/*)
elseif(APPLE)
    file(GLOB_RECURSE PLATFORM_SRC platform/macos/*)
elseif(ANDROID)
    file(GLOB_RECURSE PLATFORM_SRC platform/android/*)
endif()

sky_add_library(TARGET VulkanRHI.Static STATIC
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
        RHI
        VulkanSdk
        3rdParty::vma
        ${PLATFORM_EXT_LIBS}
)

if (SKY_BUILD_XR)
    target_compile_definitions(VulkanRHI.Static PUBLIC -DXR_USE_GRAPHICS_API_VULKAN)
endif ()

if(ANDROID)
    target_compile_definitions(VulkanRHI.Static PUBLIC -DVMA_VULKAN_VERSION=1002000)
endif()

sky_add_library(TARGET VulkanRHI SHARED
    SOURCES
        Export.cpp
    PRIVATE_INC
        include
    PUBLIC_INC
    LINK_LIBS
        VulkanRHI.Static
    )
sky_add_dependency(TARGET VulkanRHI DEPENDENCIES Editor Launcher)