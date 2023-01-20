file(GLOB_RECURSE APP_SRC src/*)
file(GLOB_RECURSE APP_INC include/*)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    file(GLOB_RECURSE PLATFORM_SRC
        platform/sdl/*
        platform/windows/*)
    set(FM_EXT_LIB
        3rdParty::sdl)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    file(GLOB_RECURSE PLATFORM_SRC
        platform/sdl/*
        platform/macos/*)
    set(FM_EXT_LIB
        3rdParty::sdl
        3rdParty::boost_container)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    find_package(game-activity REQUIRED CONFIG)
    file(GLOB_RECURSE PLATFORM_SRC
        platform/android/*)
    set(FM_EXT_LIB
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
        3rdParty::rapidjson
        3rdParty::sqlite
        ${FM_EXT_LIB}
        ${PLATFORM_EXT_LIBS}
    )
