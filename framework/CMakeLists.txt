file(GLOB_RECURSE APP_SRC src/*)
file(GLOB_RECURSE APP_INC include/*)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    file(GLOB_RECURSE PLATFORM_SRC
        platform/sdl/*
        platform/windows/*)
    set(PLATFORM_EXT_LIB 3rdParty::sdl)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    file(GLOB_RECURSE PLATFORM_SRC
        platform/sdl/*
        platform/windows/*)
    set(PLATFORM_EXT_LIB 3rdParty::sdl)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    find_package(game-activity REQUIRED CONFIG)
    file(GLOB_RECURSE PLATFORM_SRC
            platform/android/*)
    set(PLATFORM_EXT_LIB
            android
            log
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
        3rdParty::cereal
        ${PLATFORM_EXT_LIB}
    )
