file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

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
        SkyRenderAdaptor
        3rdParty::ImGuizmo
)

sky_add_dependency(TARGET ImGuizmoModule DEPENDENCIES Launcher Editor)