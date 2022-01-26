sky_add_exe(TARGET AssetTool ${PLATFORM_TYPE}
    SOURCES
        AssetTools.cpp
    )

make_directory(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)
add_custom_command(
    TARGET AssetTool POST_BUILD
    COMMAND glslc ${CMAKE_SOURCE_DIR}/shaders/PosWithUv.vert -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/PosWithUv.vert.spv
    COMMAND glslc ${CMAKE_SOURCE_DIR}/shaders/BaseColor.frag -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/BaseColor.frag.spv
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/shaders/BaseColor.prog ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/BaseColor.prog
    COMMAND glslc ${CMAKE_SOURCE_DIR}/shaders/Fullscreen.vert -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/Fullscreen.vert.spv
    COMMAND glslc ${CMAKE_SOURCE_DIR}/shaders/Fullscreen.frag -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/Fullscreen.frag.spv
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/shaders/Fullscreen.prog ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/Fullscreen.prog
    VERBATIM)