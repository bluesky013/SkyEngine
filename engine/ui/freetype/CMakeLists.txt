set(CURRENT_FOLDER Engine/UI/FreeType)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_find_3rd(TARGET freetype DIR freetype)

sky_add_library(TARGET UI.FreeType STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        UI.Interface
        Core
        3rdParty::freetype
    PUBLIC_DEFS
        SKY_BUILD_FREETYPE=1
)
