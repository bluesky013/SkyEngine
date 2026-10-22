file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

set(BUILDER_EXT_LIBS
        3rdParty::stb
#        3rdParty::ktx
        3rdParty::assimp
        3rdParty::ispc_texcomp
        3rdParty::meshoptimizer
        3rdParty::metis
    )

if (TARGET 3rdParty::usd)
    list(APPEND BUILDER_EXT_LIBS 3rdParty::usd)
endif ()

sky_add_library(TARGET RenderBuilder.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Framework
        ShaderCompiler.Static
        RenderAdaptor
        ${BUILDER_EXT_LIBS}
    )
