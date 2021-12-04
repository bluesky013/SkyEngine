file(GLOB_RECURSE TEST_SRC ./*)

sky_add_test(TARGET FrameworkTest
    SOURCES
        ${TEST_SRC}
    LIBS
        Framework
        googletest
    )