set(CURRENT_FOLDER Sandbox/Widgets)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

# Shared editor controls sit below the shell and feature editor extensions.
# They use editor services and the UI toolkit, without knowing a concrete host
# or renderer. Keep that boundary enforceable as new controls are added.
foreach (file ${SRC_FILES} ${INC_FILES})
    file(READ ${file} EDITOR_WIDGET_CONTENT)
    if (EDITOR_WIDGET_CONTENT MATCHES "#include *[<\"](editor/shell/|aurora/|render/|ui/render/)")
        message(FATAL_ERROR "EditorWidgets must not include shell or render implementation: ${file}")
    endif ()
endforeach ()

sky_add_library(TARGET EditorWidgets STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        EditorUi
        EditorCore
        Framework
        Core
        UI
)
