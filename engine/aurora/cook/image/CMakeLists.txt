file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_find_3rd(TARGET stb          DIR stb)
sky_find_3rd(TARGET ispc_texcomp DIR ispc_texcomp)
sky_find_3rd(TARGET astc         DIR astc)

sky_add_library(TARGET AuroraCook.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Aurora.Adaptor
        Framework
        Core
        3rdParty::stb
        3rdParty::ispc_texcomp
        3rdParty::astc
    )
