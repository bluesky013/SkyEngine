set(CURRENT_FOLDER Engine)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET Navigation STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Framework
)

add_subdirectory(builder)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET NavigationTest
        SOURCES
            ${TEST_FILES}
        LIBS
            Navigation
            3rdParty::googletest
        WORKING_DIR
            $<TARGET_FILE_DIR:NavigationTest>
        )
endif ()