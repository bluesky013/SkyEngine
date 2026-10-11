set(CURRENT_FOLDER Engine)

add_subdirectory(framework)
add_subdirectory(core)
add_subdirectory(animation)

# Legacy render/shader stack (pre-aurora). Deprecated, desktop-only, and not used
# by the aurora runtime or the sandbox editor. Opt in with -DSKY_BUILD_LEGACY_RENDER=ON.
if (SKY_BUILD_LEGACY_RENDER AND NOT SKY_MOBILE)
    add_subdirectory(shader)
    add_subdirectory(render)
endif ()

add_subdirectory(content)
add_subdirectory(audio)
add_subdirectory(aurora)
add_subdirectory(ui)
add_subdirectory(physics)
add_subdirectory(network)
add_subdirectory(navigation)
add_subdirectory(terrain)
add_subdirectory(vegetation)

add_subdirectory(launcher)
sky_set_dependency(TARGET Launcher)

if (SKY_BUILD_EDITOR)
    add_subdirectory(editor)
    sky_set_dependency(TARGET Editor)
endif()

# New non-Qt editor rebuild area (editor core + shell prototype + tests).
if (SKY_BUILD_SANDBOX OR SKY_BUILD_TEST)
    add_subdirectory(sandbox)
endif()

# Feature editor-extension modules (asset-view data + custom views). They depend on the
# editor framework (EditorCore / EditorUi), so they must be configured after `sandbox`.
if (SKY_BUILD_SANDBOX)
    add_subdirectory(content/editor)
    add_subdirectory(aurora/editor)
endif()

if (SKY_BUILD_SANDBOX)
    sky_set_dependency(TARGET SandboxEditor)

    # The launcher's editor mode (`Launcher --app editor`) loads the editor module,
    # so the launcher depends on SandboxModule (and its outputs deploy next to it).
    if (TARGET Launcher)
        sky_add_dependency(TARGET SandboxModule DEPENDENCIES Launcher)
        sky_set_dependency(TARGET Launcher)
    endif()
endif()

if (SKY_BUILD_TEST)
    add_subdirectory(test)
endif()