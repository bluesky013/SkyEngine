set(CURRENT_FOLDER Engine)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET Network STATIC
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
    sky_add_test(TARGET NetworkTest
        SOURCES
            ${TEST_FILES}
        LIBS
            Network
            3rdParty::googletest
        WORKING_DIR
            $<TARGET_FILE_DIR:NetworkTest>
        )
endif ()
