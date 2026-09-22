set(CURRENT_FOLDER Executable)

if (WIN32)
    file(GLOB_RECURSE PLATFORM_SRC windows/*)
elseif(APPLE)
    file(GLOB_RECURSE PLATFORM_SRC macos/*)
endif()
file(GLOB_RECURSE COMMON_SRC common/*)

if (NOT ANDROID)
    sky_add_exe(TARGET Launcher ${PLATFORM_BUNDLE}
        SOURCES
            ${PLATFORM_SRC}
            ${COMMON_SRC}
        INCS
            common
        LIBS
            Framework
    )

    # Deploy the engine builtin configs next to the executable so the launcher
    # can boot without an external project directory.
    add_custom_command(TARGET Launcher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/configs
            $<TARGET_FILE_DIR:Launcher>/configs
        COMMENT "Deploying builtin engine configs"
    )
else()
    add_subdirectory(android/app/src/main/cpp)
endif()