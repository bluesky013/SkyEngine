set(CURRENT_FOLDER Engine)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

# Content editor extension module (registration-only): registers the content adaptor's
# animation asset types + per-asset view data with the editor. Loaded via the module config.
sky_add_library(TARGET Content.Views SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PUBLIC_INC
        include
    LINK_LIBS
        Content.Adaptor
        EditorCore
        Framework
        Core
)
sky_add_dependency(TARGET Content.Views DEPENDENCIES Launcher SandboxEditor)
