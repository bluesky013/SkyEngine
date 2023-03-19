file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET SkyRender.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Framework
        RHI
    )

sky_add_library(TARGET SkyRender SHARED
    SOURCES
        RenderModule.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        SkyRenderAdaptor
    )

add_subdirectory(adaptor)
sky_add_dependency(TARGET SkyRender DEPENDENCIES Launcher Editor)