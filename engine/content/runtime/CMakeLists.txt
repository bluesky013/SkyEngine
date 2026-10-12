set(CURRENT_FOLDER Engine)

file(GLOB_RECURSE MODULE_SRC module/*)

sky_add_module(TARGET Content.Module
    SOURCES
        ${MODULE_SRC}
    LINK_LIBS
        Content.Adaptor
        Framework
)
sky_add_dependency(TARGET Content.Module DEPENDENCIES Launcher SandboxEditor)
