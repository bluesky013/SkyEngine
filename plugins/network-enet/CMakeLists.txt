file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

include(${ENGINE_ROOT}/cmake/plugin_config.cmake)

sky_find_3rd(TARGET enet DIR enet)

sky_add_library(TARGET EnetNetwork SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        EnetModule.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Network
        Framework
        3rdParty::enet
)

sky_plugin_apply_target_dependencies(CONFIG "${CMAKE_CURRENT_LIST_DIR}/plugin.json")

if (SKY_BUILD_TEST)
    file(GLOB_RECURSE TEST_FILES test/*)
    sky_add_test(TARGET EnetNetworkTest
        SOURCES
            ${TEST_FILES}
            ${SRC_FILES}
            EnetModule.cpp
        INCS
            include
            src
            ${ENGINE_ROOT}/engine/network/test
        LIBS
            Network
            Framework
            3rdParty::enet
            3rdParty::googletest
        WORKING_DIR
            $<TARGET_FILE_DIR:EnetNetworkTest>
        )
endif ()
