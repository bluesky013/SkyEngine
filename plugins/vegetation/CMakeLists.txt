set(CURRENT_FOLDER Plugins)

file(GLOB_RECURSE RUNTIME_SRC_FILES runtime/src/*)
file(GLOB_RECURSE RUNTIME_INC_FILES runtime/include/*)
file(GLOB_RECURSE TEST_FILES test/*)

# Vegetation feature implementation (placement, sub-system, asset serialization, component, ...).
# Engine interfaces/data live in engine/vegetation (target Vegetation).
sky_add_library(TARGET Vegetation.Static STATIC
    SOURCES
        ${RUNTIME_SRC_FILES}
        ${RUNTIME_INC_FILES}
    PRIVATE_INC
        runtime/src
    PUBLIC_INC
        runtime/include
    LINK_LIBS
        Vegetation
        Terrain
        Framework
)

sky_add_library(TARGET VegetationRender SHARED
    SOURCES
        runtime/VegetationRegistry.cpp
    LINK_LIBS
        Vegetation.Static
)

add_subdirectory(terrain)

sky_plugin_apply_target_dependencies(CONFIG "${CMAKE_CURRENT_LIST_DIR}/plugin.json")

if (SKY_BUILD_TEST)
    sky_add_test(TARGET VegetationPluginTest
        SOURCES
            ${TEST_FILES}
        LIBS
            Vegetation.Static
            Vegetation
            VegetationTerrain
            Terrain
            Terrain.Static
            Physics
            3rdParty::googletest
        WORKING_DIR
            $<TARGET_FILE_DIR:VegetationPluginTest>
        )
endif ()
