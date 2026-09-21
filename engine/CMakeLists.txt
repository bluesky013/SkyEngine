set(CURRENT_FOLDER Engine)

add_subdirectory(framework)
add_subdirectory(core)
add_subdirectory(animation)
add_subdirectory(shader)
add_subdirectory(audio)
add_subdirectory(aurora)
add_subdirectory(render)
add_subdirectory(ui)
add_subdirectory(physics)
add_subdirectory(navigation)

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

if (SKY_BUILD_SANDBOX)
    sky_set_dependency(TARGET Sandbox)
endif()

if (SKY_BUILD_TEST)
    add_subdirectory(test)
endif()