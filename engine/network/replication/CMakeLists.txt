set(CURRENT_FOLDER Engine)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET NetworkReplication STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Network
)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET NetworkReplicationTest
        SOURCES
            ${TEST_FILES}
            ${ENGINE_ROOT}/engine/network/test/LoopbackBackend.cpp
        INCS
            ${ENGINE_ROOT}/engine/network/test
        LIBS
            NetworkReplication
            Network
            3rdParty::googletest
        WORKING_DIR
            $<TARGET_FILE_DIR:NetworkReplicationTest>
        )

    file(GLOB_RECURSE BENCH_FILES bench/*)
    if (BENCH_FILES)
        add_executable(NetworkBenchmark ${BENCH_FILES})
        target_include_directories(NetworkBenchmark PRIVATE include)
        target_link_libraries(NetworkBenchmark PRIVATE NetworkReplication Network)
    endif ()
endif ()
