file(GLOB_RECURSE TEST_SRC ./*)

sky_add_test(TARGET FrameworkTest
    SOURCES
        ${TEST_SRC}
    WORKING_DIR
        ${CMAKE_SOURCE_DIR}
    LIBS
        Framework
        3rdParty::googletest
    )