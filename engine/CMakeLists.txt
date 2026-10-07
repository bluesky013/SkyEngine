set(CURRENT_FOLDER Engine)

add_subdirectory(framework)
add_subdirectory(core)
add_subdirectory(animation)

# Legacy render/shader stack is desktop-only (the runtime renderer is aurora on
# slang). Skipped on mobile so legacy third-party (boost/glslang/SPIRV-Cross/
# taskflow) is neither found nor built there.
if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "iOS" AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    add_subdirectory(shader)
    add_subdirectory(render)
endif ()

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