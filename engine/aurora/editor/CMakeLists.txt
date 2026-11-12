file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

# Aurora editor extension module (loaded by the editor through module config).
# Replaces the former SkyRender.Editor. Links the bridge for AuroraModule.
sky_add_library(TARGET Aurora.Render.Editor SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PUBLIC_INC
        include
    PRIVATE_DEFS
        AURORA_EDITOR_IMPLEMENTATION
    LINK_LIBS
        Aurora.Adaptor.Static
        EditorCore
        EditorUi
        EditorWidgets
        UI
        Framework
        Core
        Physics.SoftBody.Interface
)

sky_add_dependency(TARGET Aurora.Render.Editor DEPENDENCIES Launcher Editor SandboxEditor)
