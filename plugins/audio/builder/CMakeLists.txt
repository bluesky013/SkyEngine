set(CURRENT_FOLDER Plugins)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_find_3rd(TARGET miniaudio DIR miniaudio)

sky_add_library(TARGET AudioBuilder.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Audio
        Framework
        3rdParty::miniaudio
)

file(GLOB_RECURSE MODULE_SRC module/*)

sky_add_library(TARGET SkyAudio.Builder SHARED
    SOURCES
        ${MODULE_SRC}
    LINK_LIBS
        AudioBuilder.Static
)
sky_add_dependency(TARGET SkyAudio.Builder DEPENDENCIES Launcher)
