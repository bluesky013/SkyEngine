file(GLOB_RECURSE TEST_SRC ./*)

sky_add_exe(TARGET CoreTest
    SOURCES
        ${TEST_SRC}
    LIBS
        Core
        googletest
    )