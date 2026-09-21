file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

include(${ENGINE_ROOT}/cmake/plugin_config.cmake)

sky_find_3rd(TARGET recast DIR recast)

sky_add_library(TARGET RecastNavigation SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        RecastModule.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Navigation
        Physics
        3rdParty::recast
)

sky_plugin_apply_target_dependencies(CONFIG "${CMAKE_CURRENT_LIST_DIR}/plugin.json")

if (SKY_BUILD_TEST)
    file(GLOB_RECURSE TEST_FILES test/*)
    sky_add_test(TARGET RecastModuleTest
        SOURCES
            ${TEST_FILES}
        LIBS
            Navigation
            3rdParty::googletest
        WORKING_DIR
            $<TARGET_FILE_DIR:RecastNavigation>
        )
    add_dependencies(RecastModuleTest RecastNavigation)
endif ()