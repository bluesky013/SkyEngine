add_subdirectory(shader)
add_subdirectory(prefab)

sky_add_library(TARGET BuiltinBuilder SHARED
    SOURCES
        module/BuilderModule.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        PrefabBuilder
        ShaderBuilder
    )
add_dependencies(Launcher BuiltinBuilder)
add_dependencies(Editor BuiltinBuilder)