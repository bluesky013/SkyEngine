set(CURRENT_FOLDER Sandbox/Shell)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

# UI-linked editor shell. Composes the sky::ui element tree from the
# render-independent EditorCore layout model + panel registry. Kept out of
# EditorCore (which is UI-free) and out of EditorRender (which is the RHI host).
sky_add_library(TARGET EditorShell STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        Framework
        EditorCore
        UI
)
