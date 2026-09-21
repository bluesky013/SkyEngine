set(CURRENT_FOLDER Engine)

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
