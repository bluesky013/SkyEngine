file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

include(${ENGINE_ROOT}/cmake/plugin_config.cmake)

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
        RenderAdaptor
        Physics
        3rdParty::recast
)

sky_plugin_apply_target_dependencies(CONFIG "${CMAKE_CURRENT_LIST_DIR}/plugin.json")