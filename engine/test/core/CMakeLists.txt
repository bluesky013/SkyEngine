file(GLOB_RECURSE TEST_SRC ./*)

sky_add_test(TARGET CoreTest
    SOURCES
        ${TEST_SRC}
    LIBS
        Core
        3rdParty::googletest
    )