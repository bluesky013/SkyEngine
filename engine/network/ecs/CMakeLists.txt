set(CURRENT_FOLDER Engine)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET NetworkEcs STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        NetworkReplication
        Core
)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET NetworkEcsTest
        SOURCES
            ${TEST_FILES}
            ${ENGINE_ROOT}/engine/network/test/LoopbackBackend.cpp
        INCS
            ${ENGINE_ROOT}/engine/network/test
        LIBS
            NetworkEcs
            NetworkReplication
            Network
            Core
            3rdParty::googletest
        WORKING_DIR
            $<TARGET_FILE_DIR:NetworkEcsTest>
        )
endif ()
