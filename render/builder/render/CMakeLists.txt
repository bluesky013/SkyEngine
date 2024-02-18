file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

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
        SkyRenderAdaptor
        3rdParty::stb
        3rdParty::assimp
        3rdParty::ispc_texcomp
        3rdParty::meshoptimizer
    )

sky_add_dependency(TARGET RenderBuilder DEPENDENCIES AssetTool Editor)
