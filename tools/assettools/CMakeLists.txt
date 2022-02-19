sky_add_exe(TARGET AssetTool ${PLATFORM_TYPE}
    SOURCES
        AssetTools.cpp
    LIBS
        ModelLoader
        ShaderLoader
    )

make_directory(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)
make_directory(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models)

add_custom_command(
    TARGET AssetTool PRE_BUILD
    COMMAND glslc ${CMAKE_SOURCE_DIR}/shaders/BaseColor.vert -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/BaseColor.vert.spv
    COMMAND glslc ${CMAKE_SOURCE_DIR}/shaders/BaseColor.frag -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/BaseColor.frag.spv
    COMMAND glslc ${CMAKE_SOURCE_DIR}/shaders/Fullscreen.vert -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/Fullscreen.vert.spv
    COMMAND glslc ${CMAKE_SOURCE_DIR}/shaders/Fullscreen.frag -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/Fullscreen.frag.spv
    VERBATIM)