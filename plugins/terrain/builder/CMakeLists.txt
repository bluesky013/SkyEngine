set(CURRENT_FOLDER Engine)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET TerrainBuilder.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Terrain.Static
        Framework
)

file(GLOB_RECURSE MODULE_SRC module/*)

sky_add_library(TARGET SkyTerrain.Builder SHARED
    SOURCES
        ${MODULE_SRC}
    LINK_LIBS
        TerrainBuilder.Static
)

sky_add_dependency(TARGET SkyTerrain.Builder DEPENDENCIES Launcher Editor)
