set(CURRENT_FOLDER Plugins)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET NavigationBuilder.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        RecastNavigation.Shared.Static
        Navigation
        Framework
)

file(GLOB_RECURSE MODULE_SRC module/*)

sky_add_module(TARGET SkyNavigation.Builder
    SOURCES
        ${MODULE_SRC}
    LINK_LIBS
        NavigationBuilder.Static
)
sky_add_dependency(TARGET SkyNavigation.Builder DEPENDENCIES Launcher SandboxEditor AssetTool)

add_dependencies(SkyNavigation.Builder RecastNavigation)
