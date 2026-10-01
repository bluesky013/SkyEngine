set(CURRENT_FOLDER Engine)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET NetworkLockstep STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        NetworkReplication
        Network
)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET NetworkLockstepTest
        SOURCES
            ${TEST_FILES}
            ${ENGINE_ROOT}/engine/network/test/LoopbackBackend.cpp
        INCS
            ${ENGINE_ROOT}/engine/network/test
        LIBS
            NetworkLockstep
            NetworkReplication
            Network
            3rdParty::googletest
        WORKING_DIR
            $<TARGET_FILE_DIR:NetworkLockstepTest>
        )
endif ()
