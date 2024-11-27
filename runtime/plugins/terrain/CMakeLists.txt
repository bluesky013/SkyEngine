file(GLOB_RECURSE RUNTIME_SRC_FILES runtime/src/*)
file(GLOB_RECURSE RUNTIME_INC_FILES runtime/include/*)

file(GLOB_RECURSE TOOL_SRC_FILES tools/src/*)
file(GLOB_RECURSE TOOL_INC_FILES tools/include/*)

file(GLOB_RECURSE EDITOR_SRC_FILES editor/src/*)
file(GLOB_RECURSE EDITOR_INC_FILES editor/include/*)

file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET Terrain.Static STATIC
    SOURCES
        ${RUNTIME_SRC_FILES}
        ${RUNTIME_INC_FILES}
    PRIVATE_INC
        runtime/src
    PUBLIC_INC
        runtime/include
    LINK_LIBS
        RenderAdaptor
        Physics
)

sky_add_library(TARGET Terrain SHARED
    SOURCES
        runtime/TerrainRegistry.cpp
    PRIVATE_INC
    PUBLIC_INC
    LINK_LIBS
        Terrain.Static
)

sky_add_dependency(TARGET Terrain DEPENDENCIES Launcher)

if (SKY_BUILD_EDITOR)
    sky_add_library(TARGET TerrainEditor SHARED
        SOURCES
            ${EDITOR_SRC_FILES}
            ${EDITOR_INC_FILES}
            editor/TerrainEditorRegistry.cpp
        PRIVATE_INC
            editor/src
        PUBLIC_INC
            editor/include
        LINK_LIBS
            Terrain.Static
            EditorFramework
            3rdParty::PerlinNoise
    )
    sky_add_dependency(TARGET TerrainEditor DEPENDENCIES Editor)
endif ()

sky_add_library(TARGET TerrainTools STATIC
    SOURCES
        ${TOOL_SRC_FILES}
        ${TOOL_INC_FILES}
    PRIVATE_INC
        tools/src
    PUBLIC_INC
        tools/include
   LINK_LIBS
        Terrain.Static
        3rdParty::PerlinNoise
)

sky_add_test(TARGET TerrainTest
    SOURCES
        ${TEST_FILES}
    LIBS
        Terrain.Static
        3rdParty::googletest
)