set(CURRENT_FOLDER Engine)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

# Terrain surface bridge: implements the vegetation surface provider from terrain LOD0 data and
# forwards terrain tile changes as surface-change notifications. Links Terrain + Vegetation.
sky_add_library(TARGET VegetationTerrain STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Vegetation
        Terrain
        Framework
)
