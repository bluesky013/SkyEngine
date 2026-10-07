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

# Absolute path to the project templates for ProjectScaffoldTest, so it works
# regardless of the working directory the test binary is launched from.
if (TARGET FrameworkTest)
    target_compile_definitions(FrameworkTest PRIVATE
        SKY_TEST_TEMPLATE_DIR="${CMAKE_SOURCE_DIR}/engine/templates")
endif ()
