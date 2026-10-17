set(CURRENT_FOLDER Sandbox)

# Non-Qt editor. `core` is the toolkit- and render-independent editor core;
# `render` is the editor renderer; `module` is the editor module (loaded by
# ModuleManager); `app` is the non-Qt host.
add_subdirectory(core)

if (SKY_BUILD_SANDBOX)
    # Editor UI primitives + the UI-facing extension seam, shared with feature editor
    # modules so they can provide custom `UIElement` views without linking the shell.
    add_subdirectory(ui)

    # Reusable reflected controls and editor chrome. Feature extensions can
    # link these without depending on the shell's host/window composition.
    add_subdirectory(widgets)

    # UI-linked shell: composes EditorCore into a sky::ui tree.
    add_subdirectory(shell)

    # Editor-owned renderer (Aurora + UI); draws the shell's draw data.
    add_subdirectory(render)

    # Editor module (dynamic) + non-Qt host that loads it.
    add_subdirectory(module)
    add_subdirectory(app)
endif ()

if (SKY_BUILD_TEST)
    add_subdirectory(test)
endif ()
