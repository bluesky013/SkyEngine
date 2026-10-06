set(CURRENT_FOLDER Engine/Foundation)

file(GLOB_RECURSE APP_SRC src/*)
file(GLOB_RECURSE APP_INC include/*)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    file(GLOB_RECURSE PLATFORM_SRC
        platform/windows/*)

    set(FM_EXT_LIB
        )
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    # NOTE: the macOS window backend (previously SDL-based) has been removed with
    # the SDL dependency. macOS is unsupported until a native backend is added;
    # GeneticPerfManager (cross-platform) remains.
    file(GLOB_RECURSE PLATFORM_SRC
        platform/genetic/*
        platform/posix/*)
    set(FM_EXT_LIB
        )
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    find_package(game-activity REQUIRED CONFIG)

    file(GLOB_RECURSE PLATFORM_SRC
        platform/android/*)
    set(FM_EXT_LIB
        log
        android
        game-activity::game-activity)
endif ()

sky_add_library(TARGET Framework STATIC
    SOURCES
        ${PLATFORM_SRC}
        ${APP_SRC}
        ${APP_INC}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        ${FM_EXT_LIB}
        ${PLATFORM_EXT_LIBS}
    INSTALL_DIR
        include
    )
