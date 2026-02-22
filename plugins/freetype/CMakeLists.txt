file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET FreeTypeModule SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        FreeTypeModule.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Framework
        RenderCore
        3rdParty::freetype
)

sky_add_dependency(TARGET FreeTypeModule DEPENDENCIES Launcher Editor)