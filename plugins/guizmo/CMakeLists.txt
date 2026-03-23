file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

include(${ENGINE_ROOT}/cmake/plugin_config.cmake)

sky_find_3rd(TARGET ImGuizmo DIR ImGuizmo)

sky_add_library(TARGET ImGuizmoModule SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        ImGuizmoModule.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Framework
        ImGuiRender
        RenderAdaptor
        3rdParty::ImGuizmo
)

sky_plugin_apply_target_dependencies(CONFIG "${CMAKE_CURRENT_LIST_DIR}/plugin.json")