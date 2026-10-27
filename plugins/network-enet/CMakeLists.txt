file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

include(${ENGINE_ROOT}/cmake/plugin_config.cmake)

# enet's clock_gettime conflicts with the modern iOS SDK; skip the enet transport
# plugin on mobile for now (networking on mobile is a follow-up).
if (SKY_MOBILE)
    return()
endif ()

sky_find_3rd(TARGET enet DIR enet)

sky_add_library(TARGET EnetNetwork.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Network
        Framework
        3rdParty::enet
)
set_target_properties(EnetNetwork.Static PROPERTIES POSITION_INDEPENDENT_CODE ON)
sky_add_module(TARGET EnetNetwork
    SOURCES EnetModule.cpp
    LINK_LIBS EnetNetwork.Static
)

sky_plugin_apply_target_dependencies(CONFIG "${CMAKE_CURRENT_LIST_DIR}/plugin.json")

if (SKY_BUILD_TEST)
    file(GLOB_RECURSE TEST_FILES test/*)
    sky_add_test(TARGET EnetNetworkTest
        SOURCES
            ${TEST_FILES}
        INCS
            src
            ${ENGINE_ROOT}/engine/network/test
        LIBS
            EnetNetwork.Static
            3rdParty::googletest
        WORKING_DIR
            $<TARGET_FILE_DIR:EnetNetworkTest>
        )
endif ()
