set(CURRENT_FOLDER Engine)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET NavigationTerrain STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Navigation
        Terrain
        Framework
)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET NavigationTerrainTest
        SOURCES
            ${TEST_FILES}
        LIBS
            NavigationTerrain
            Navigation
            Terrain
            3rdParty::googletest
        WORKING_DIR
            $<TARGET_FILE_DIR:NavigationTerrainTest>
        )
endif ()
