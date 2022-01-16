file(GLOB_RECURSE LOADER_SRC src/*)
file(GLOB_RECURSE LOADER_INC include/*)

sky_add_library(TARGET ShaderLoader SHARED
    SOURCES
        ${LOADER_SRC}
        ${LOADER_INC}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Tinygltf
        SkyEngine
    )

make_directory(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)
add_custom_command(
    TARGET ShaderLoader PRE_BUILD
    COMMAND glslc ${CMAKE_SOURCE_DIR}/shaders/PosWithUv.vert -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/PosWithUv.vert.spv
    COMMAND glslc ${CMAKE_SOURCE_DIR}/shaders/BaseColor.frag -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/BaseColor.frag.spv
    VERBATIM)