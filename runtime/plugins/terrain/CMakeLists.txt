file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)
file(GLOB_RECURSE TOOL_SRC_FILES tools/src/*)
file(GLOB_RECURSE TOOL_INC_FILES tools/include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET Terrain.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        RenderAdaptor
        Physics
)

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

sky_add_library(TARGET Terrain SHARED
    SOURCES
        TerrainModule.cpp
    PRIVATE_INC
    PUBLIC_INC
    LINK_LIBS
        Terrain.Static
        TerrainTools
)

sky_add_test(TARGET TerrainTest
    SOURCES
        ${TEST_FILES}
    LIBS
        Terrain.Static
        3rdParty::googletest
)

sky_add_dependency(TARGET Terrain DEPENDENCIES Launcher Editor)