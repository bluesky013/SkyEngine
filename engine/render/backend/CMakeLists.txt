add_subdirectory(rhi)
add_subdirectory(vulkan)

if (WIN32)
    add_subdirectory(dx12)
endif()

if (APPLE)
    add_subdirectory(metal)
endif()

if (SKY_BUILD_GLES OR ANDROID)
    #add_subdirectory(gles)
endif()
