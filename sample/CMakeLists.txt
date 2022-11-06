add_subdirectory(VulkanSampleBase)

if (APPLE)
    add_subdirectory(MetalSampleBase)
endif()

if (WIN32)
    add_subdirectory(DXSampleBase)
endif()


add_subdirectory(RDSceneProject)
