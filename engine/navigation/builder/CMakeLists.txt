set(CURRENT_FOLDER Engine)

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
        Navigation
        Framework
)

file(GLOB_RECURSE MODULE_SRC module/*)

sky_add_library(TARGET SkyNavigation.Builder SHARED
    SOURCES
        ${MODULE_SRC}
    LINK_LIBS
        NavigationBuilder.Static
)
sky_add_dependency(TARGET SkyNavigation.Builder DEPENDENCIES Launcher)
