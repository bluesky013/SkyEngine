set(CURRENT_FOLDER Runtime/Foundation)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET Animation STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET AnimationTest
        SOURCES
            ${TEST_FILES}
        LIBS
            Animation
            3rdParty::googletest
            )
endif ()
