set(CURRENT_FOLDER Sandbox/Ui)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

# Editor UI primitives + the UI-facing extension seam (asset-view provider). Kept below
# EditorShell so feature editor-extension modules can provide custom `UIElement` views
# without linking the whole shell. UI-dependent, hence separate from the UI-free EditorCore.
sky_add_library(TARGET EditorUi STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        UI
)
