file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

# the legacy render dependency graph uses boost::graph
sky_find_3rd(TARGET boost DIR boost)

sky_add_library(TARGET RenderCore STATIC
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
        ShaderCompiler.Static
        3rdParty::boost
    INSTALL_DIR
        include
)