file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

set(TARGET_NAME RDSceneProject)

configure_file(ProjectRoot.h.in ${CMAKE_BINARY_DIR}/Gen/${TARGET_NAME}/ProjectRoot.h)
include_directories(${CMAKE_BINARY_DIR}/Gen)

sky_add_library(TARGET ${TARGET_NAME} SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
        ${CMAKE_BINARY_DIR}/Gen/${TARGET_NAME}
    PUBLIC_INC
        include
    LINK_LIBS
        Framework
        ImGuiRender
        SkyRenderAdaptor
    )
add_custom_target(${TARGET_NAME}_RES
    COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_SOURCE_DIR}/assets/techniques/volumes/volume_simple.tech"
    COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_SOURCE_DIR}/assets/techniques/geometry.tech"
    COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_SOURCE_DIR}/assets/techniques/gui.tech"
    COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_SOURCE_DIR}/assets/techniques/post_processing.tech"
    COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_SOURCE_DIR}/assets/techniques/skybox.tech"
    COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_SOURCE_DIR}/assets/vertex/vertex_library.vtxlib"
    COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/DamagedHelmet.glb"
    COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_CURRENT_SOURCE_DIR}/assets/materials/volume_simple.mati"
    COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_CURRENT_SOURCE_DIR}/assets/materials/floor.mati"
    COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_CURRENT_SOURCE_DIR}/assets/skybox/output_skybox.dds"
    COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_CURRENT_SOURCE_DIR}/assets/skybox/output_iem.dds"
    COMMAND AssetTool -e ${CMAKE_SOURCE_DIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -o "${CMAKE_CURRENT_SOURCE_DIR}/assets/skybox/output_pmrem.dds"
    VERBATIM
    )

sky_add_dependency(TARGET ${TARGET_NAME} DEPENDENCIES Launcher AssetTool)