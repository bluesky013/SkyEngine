file(GLOB_RECURSE TEST_SRC ./*)

sky_add_test(TARGET RenderTest
    SOURCES
        ${TEST_SRC}
    LIBS
        SkyRender
        3rdParty::googletest
    )