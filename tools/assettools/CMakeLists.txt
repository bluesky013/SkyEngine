sky_add_exe(TARGET AssetTool ${PLATFORM_TYPE}
    SOURCES
        AssetTools.cpp
    )

make_directory(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)
add_custom_command(
    TARGET AssetTool PRE_BUILD
    COMMAND glslc ${CMAKE_SOURCE_DIR}/shaders/PosWithUv.vert -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/PosWithUv.vert.spv
    COMMAND glslc ${CMAKE_SOURCE_DIR}/shaders/BaseColor.frag -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/BaseColor.frag.spv
    VERBATIM)