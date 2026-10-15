# {{PROJECT_NAME}} game module.
#
# Uses the engine's dynamic/static module packaging and registration contract.
file(GLOB_RECURSE GAME_SRC src/*.cpp)
file(GLOB_RECURSE GAME_INC include/*.h)

sky_add_module(TARGET {{PROJECT_NAME}}Module
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
