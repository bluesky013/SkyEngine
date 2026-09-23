set(CURRENT_FOLDER Plugins)

file(GLOB_RECURSE RUNTIME_SRC_FILES runtime/src/*)
file(GLOB_RECURSE RUNTIME_INC_FILES runtime/include/*)
file(GLOB_RECURSE TEST_FILES test/*)

# Terrain feature implementation (field, sub-system, generator, asset serialization, component, ...).
# Engine interfaces/data live in engine/terrain (target Terrain).
sky_add_library(TARGET Terrain.Static STATIC
    SOURCES
        ${RUNTIME_SRC_FILES}
        ${RUNTIME_INC_FILES}
    PRIVATE_INC
        runtime/src
    PUBLIC_INC
        runtime/include
    LINK_LIBS
        Terrain
        Physics
        Framework
)

sky_add_library(TARGET TerrainRender SHARED
    SOURCES
        runtime/TerrainRegistry.cpp
    LINK_LIBS
        Terrain.Static
)

add_subdirectory(builder)
add_subdirectory(physics)

sky_plugin_apply_target_dependencies(CONFIG "${CMAKE_CURRENT_LIST_DIR}/plugin.json")

if (SKY_BUILD_TEST)
    sky_add_test(TARGET TerrainPluginTest
        SOURCES
            ${TEST_FILES}
        LIBS
            Terrain.Static
            Terrain
            Physics
            Navigation
            NavigationTerrain
            Vegetation
            VegetationTerrain
            3rdParty::googletest
        WORKING_DIR
            $<TARGET_FILE_DIR:TerrainPluginTest>
        )
endif ()
