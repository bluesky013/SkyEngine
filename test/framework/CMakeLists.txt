file(GLOB_RECURSE TEST_SRC ./*)

message("test framework" ${CMAKE_SOURCE_DIR})

sky_add_test(TARGET FrameworkTest
    SOURCES
        ${TEST_SRC}
    WORKING_DIR
        ${CMAKE_SOURCE_DIR}
    LIBS
        Framework
        3rdParty::googletest
    )