file(GLOB_RECURSE SRC_FILES ./*)

set(TARGET_NAME NoiseGenerator)

configure_file(ProjectRoot.h.in ${CMAKE_BINARY_DIR}/Gen/${TARGET_NAME}/ProjectRoot.h)
include_directories(${CMAKE_BINARY_DIR}/Gen)

sky_add_library(TARGET ${TARGET_NAME} SHARED
    SOURCES
        ${SRC_FILES}
    PRIVATE_INC
        src
        ${CMAKE_BINARY_DIR}/Gen/${TARGET_NAME}
    PUBLIC_INC
        include
    LINK_LIBS
        Framework
        ImGuiRender
        SkyRenderAdaptor
        3rdParty::PerlinNoise
)

add_custom_target(${TARGET_NAME}_RES
        COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_SOURCE_DIR}/assets/techniques/volumes/volume_simple.tech"
        COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_SOURCE_DIR}/assets/techniques/gui.tech"
        COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_SOURCE_DIR}/assets/techniques/geometry.tech"
        COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_SOURCE_DIR}/assets/vertex/vertex_library.vtxlib"
        COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_SOURCE_DIR}/assets/shapes/plane.glb"
        COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_CURRENT_SOURCE_DIR}/assets/noise.mat"
        VERBATIM
)

sky_add_dependency(TARGET ${TARGET_NAME} DEPENDENCIES Launcher)