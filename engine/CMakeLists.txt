set(CURRENT_FOLDER Engine)

add_subdirectory(framework)
add_subdirectory(core)
add_subdirectory(animation)
add_subdirectory(render)
add_subdirectory(physics)
add_subdirectory(navigation)

add_subdirectory(launcher)
sky_set_dependency(TARGET Launcher)

if (SKY_BUILD_EDITOR)
    add_subdirectory(editor)
    sky_set_dependency(TARGET Editor)
endif()

if (SKY_BUILD_TEST)
    add_subdirectory(test)
endif()