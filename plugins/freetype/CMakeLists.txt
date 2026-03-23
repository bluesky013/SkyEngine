file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

include(${ENGINE_ROOT}/cmake/plugin_config.cmake)

sky_find_3rd(TARGET freetype DIR freetype)

sky_add_library(TARGET FreeTypeModule SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        FreeTypeModule.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Framework
        RenderCore
        3rdParty::freetype
)

sky_plugin_apply_target_dependencies(CONFIG "${CMAKE_CURRENT_LIST_DIR}/plugin.json")