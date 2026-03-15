file(GLOB_RECURSE RUNTIME_SRC_FILES runtime/src/*)
file(GLOB_RECURSE RUNTIME_INC_FILES runtime/include/*)

file(GLOB_RECURSE EDITOR_SRC_FILES editor/src/*)
file(GLOB_RECURSE EDITOR_INC_FILES editor/include/*)

file(GLOB_RECURSE TEST_FILES test/*)

include(${ENGINE_ROOT}/cmake/plugin_config.cmake)

sky_add_library(TARGET PVS.Static STATIC
    SOURCES
        ${RUNTIME_SRC_FILES}
        ${RUNTIME_INC_FILES}
    PRIVATE_INC
        runtime/src
    PUBLIC_INC
        runtime/include
    LINK_LIBS
        Framework
        RenderCore
)

sky_add_library(TARGET PVS SHARED
    SOURCES
        runtime/Registry.cpp
    PRIVATE_INC
    PUBLIC_INC
    LINK_LIBS
        PVS.Static

)
if (SKY_BUILD_EDITOR)
    sky_add_library(TARGET PVS.Editor SHARED
        SOURCES
            ${EDITOR_SRC_FILES}
            ${EDITOR_INC_FILES}
            editor/Registry.cpp
        PRIVATE_INC
            editor/src
        PUBLIC_INC
            editor/include
        LINK_LIBS
            PVS.Static
            EditorFramework
    )
endif ()

sky_plugin_apply_target_dependencies(CONFIG "${CMAKE_CURRENT_LIST_DIR}/plugin.json")

sky_add_test(TARGET PVSTest
    SOURCES
        ${TEST_FILES}
    LIBS
        PVS.Static
        3rdParty::googletest
)