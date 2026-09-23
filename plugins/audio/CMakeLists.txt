file(GLOB_RECURSE SRC_FILES src/*)

include(${ENGINE_ROOT}/cmake/plugin_config.cmake)

sky_find_3rd(TARGET miniaudio DIR miniaudio)

sky_add_library(TARGET AudioBackend.Static STATIC
    SOURCES
        ${SRC_FILES}
    PRIVATE_INC
        src
    LINK_LIBS
        Audio
        3rdParty::miniaudio
)

sky_add_library(TARGET AudioModule SHARED
    SOURCES
        AudioModule.cpp
    PRIVATE_INC
        src
    LINK_LIBS
        AudioBackend.Static
        Framework
)

add_subdirectory(builder)

sky_plugin_apply_target_dependencies(CONFIG "${CMAKE_CURRENT_LIST_DIR}/plugin.json")

if (SKY_BUILD_TEST)
    file(GLOB_RECURSE TEST_FILES test/*)
    sky_add_test(TARGET AudioBackendTest
        SOURCES
            ${TEST_FILES}
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        LIBS
            AudioBackend.Static
            3rdParty::googletest
        WORKING_DIR
            $<TARGET_FILE_DIR:AudioModule>
        )
    add_dependencies(AudioBackendTest AudioModule)
endif ()
