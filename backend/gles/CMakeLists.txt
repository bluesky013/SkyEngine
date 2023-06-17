file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET GLESRHI.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        RHI
        3rdParty::gles
        ${PLATFORM_EXT_LIBS}
    )

sky_add_library(TARGET GLESRHI SHARED
    SOURCES
        Export.cpp
    PRIVATE_INC
        include
    PUBLIC_INC
    LINK_LIBS
        GLESRHI.Static
    )

sky_add_dependency(TARGET GLESRHI DEPENDENCIES Launcher Editor)
