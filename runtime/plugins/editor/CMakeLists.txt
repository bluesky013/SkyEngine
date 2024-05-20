file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET EditorModule SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        EditorModule.cpp
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

sky_add_dependency(TARGET EditorModule DEPENDENCIES Launcher)