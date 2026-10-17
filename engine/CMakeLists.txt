set(CURRENT_FOLDER Engine)

add_subdirectory(framework)
add_subdirectory(core)
add_subdirectory(animation)

add_subdirectory(content)
add_subdirectory(audio)
add_subdirectory(aurora)
add_subdirectory(ui)
add_subdirectory(physics)
add_subdirectory(network)
add_subdirectory(navigation)
add_subdirectory(terrain)
add_subdirectory(vegetation)

# The launcher is no longer an engine target: it is project-owned (see the
# `add-project-native-layout` change). Engine/plugin targets still register
# `DEPENDENCIES Launcher`, which the project's native entry binds via
# `sky_set_dependency(TARGET Launcher)`.

# Non-Qt editor (editor core + shell + tests).
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
endif()