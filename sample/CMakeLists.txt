add_subdirectory(VulkanSampleBase)
add_subdirectory(VulkanTriangle)
add_subdirectory(VulkanMemory)
add_subdirectory(VulkanDescriptor)
add_subdirectory(VulkanBindless)
add_subdirectory(VulkanAsyncUpload)
add_subdirectory(VulkanMemoryAliasing)

if (APPLE)
    add_subdirectory(MetalSampleBase)
endif()

add_subdirectory(RDSceneProject)
