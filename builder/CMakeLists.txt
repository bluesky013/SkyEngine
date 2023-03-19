add_subdirectory(shader)
add_subdirectory(render)

sky_add_library(TARGET BuiltinBuilder SHARED
    SOURCES
        module/BuilderModule.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        RenderBuilder
    )
add_dependencies(Launcher BuiltinBuilder)
add_dependencies(Editor BuiltinBuilder)
