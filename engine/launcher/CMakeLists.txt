set(CURRENT_FOLDER Executable)

if (WIN32)
    file(GLOB_RECURSE PLATFORM_SRC windows/*)
elseif(APPLE)
    file(GLOB_RECURSE PLATFORM_SRC macos/*)
endif()
file(GLOB_RECURSE COMMON_SRC common/*)

if (NOT ANDROID)
    # When the sandbox editor is built, compile the editor host into the launcher
    # so `Launcher --app editor` can run the editor (same host as SandboxEditor).
    set(LAUNCHER_EXTRA_SRC "")
    set(LAUNCHER_EXTRA_INC "")
    if (SKY_BUILD_SANDBOX)
        list(APPEND LAUNCHER_EXTRA_SRC ${CMAKE_SOURCE_DIR}/engine/sandbox/app/src/EditorApplication.cpp)
        list(APPEND LAUNCHER_EXTRA_INC ${CMAKE_SOURCE_DIR}/engine/sandbox/app/src)
    endif()

    sky_add_exe(TARGET Launcher ${PLATFORM_BUNDLE}
        SOURCES
            ${PLATFORM_SRC}
            ${COMMON_SRC}
            ${LAUNCHER_EXTRA_SRC}
        INCS
            common
            ${LAUNCHER_EXTRA_INC}
        LIBS
            Framework
    )

    if (SKY_BUILD_SANDBOX)
        target_link_libraries(Launcher Core)
        target_compile_definitions(Launcher PRIVATE SKY_EDITOR_HOST)
    endif()

    # Deploy the engine builtin configs next to the executable so the launcher
    # can boot without an external project directory.
    add_custom_command(TARGET Launcher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/configs
            $<TARGET_FILE_DIR:Launcher>/configs
        COMMENT "Deploying builtin engine configs"
    )

    # When the sandbox editor is built, deploy its builtin resources too so
    # `Launcher --app editor` resolves the editor fonts/icons.
    if (SKY_BUILD_SANDBOX)
        add_custom_command(TARGET Launcher POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                "-DSRC_DIR=${CMAKE_SOURCE_DIR}/engine/sandbox/resources"
                "-DDST_DIR=$<TARGET_FILE_DIR:Launcher>/resources"
                -P ${CMAKE_SOURCE_DIR}/engine/sandbox/app/deploy_resources.cmake
            COMMENT "Deploying editor resources next to the launcher"
        )
    endif()
else()
    add_subdirectory(android/app/src/main/cpp)
endif()
