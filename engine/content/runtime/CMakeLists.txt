set(CURRENT_FOLDER Engine)

sky_add_module(TARGET Content.Metadata
    SOURCES metadata/ContentMetadata.cpp
    LINK_LIBS Content.Adaptor Framework
)
sky_add_dependency(TARGET Content.Metadata DEPENDENCIES Launcher SandboxEditor AssetTool)

file(GLOB_RECURSE MODULE_SRC module/*)

sky_add_module(TARGET Content.Module
    SOURCES
        ${MODULE_SRC}
    LINK_LIBS
        Content.Adaptor
        Framework
)
sky_add_dependency(TARGET Content.Module DEPENDENCIES Launcher SandboxEditor)
