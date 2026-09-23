set(CURRENT_FOLDER Engine)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET TerrainPhysics STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Terrain.Static
        Physics
        Framework
)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET TerrainPhysicsTest
        SOURCES
            ${TEST_FILES}
        LIBS
            TerrainPhysics
            Terrain
            Physics
            3rdParty::googletest
        WORKING_DIR
            $<TARGET_FILE_DIR:TerrainPhysicsTest>
        )
endif ()
