set(CURRENT_FOLDER Engine)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET Content.Adaptor STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Animation
        Framework
)

file(GLOB_RECURSE TEST_FILES test/*)
if (SKY_BUILD_TEST)
    sky_add_test(TARGET Content.AdaptorTest
        SOURCES
            ${TEST_FILES}
        LIBS
            Content.Adaptor
            Framework
            3rdParty::googletest
    )
endif ()
