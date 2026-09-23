set(CURRENT_FOLDER Sandbox)

# Non-Qt editor rebuild area. `core` is the toolkit- and render-independent
# editor core; `render` is the editor renderer; `module` is the editor module
# (loaded by ModuleManager); `app` is the non-Qt host. Nothing here depends on
# the legacy Qt editor (engine/editor).
add_subdirectory(core)

if (SKY_BUILD_SANDBOX)
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
