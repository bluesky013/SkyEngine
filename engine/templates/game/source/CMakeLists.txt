# {{PROJECT_NAME}} game module.
#
# Built as a SHARED library exporting StartModule/StopModule (REGISTER_MODULE),
# loaded by the launcher/editor ModuleManager.
file(GLOB_RECURSE GAME_SRC src/*.cpp)
file(GLOB_RECURSE GAME_INC include/*.h)

sky_add_library(TARGET {{PROJECT_NAME}}Module SHARED
    SOURCES
        ${GAME_SRC}
        ${GAME_INC}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Framework
)

# Deploy next to the launcher so ModuleManager can load it by name.
sky_add_dependency(TARGET {{PROJECT_NAME}}Module DEPENDENCIES Launcher)
