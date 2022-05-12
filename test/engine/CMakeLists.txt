file(GLOB_RECURSE TEST_SRC ./*)

sky_add_test(TARGET EngineTest
    SOURCES
        ${TEST_SRC}
    LIBS
        SkyEngine
        3rdParty::googletest
    )