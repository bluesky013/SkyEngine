file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

set(TARGET_NAME RDSceneProject)

configure_file(EngineRoot.h.in ${CMAKE_BINARY_DIR}/Gen/${TARGET_NAME}/EngineRoot.h)
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
        SkyRender
        Framework
    )

add_dependencies(Launcher ${TARGET_NAME})