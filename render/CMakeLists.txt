add_subdirectory(backend)
add_subdirectory(core)
add_subdirectory(adaptor)
add_subdirectory(builder)
add_subdirectory(imgui)

sky_add_library(TARGET SkyRender SHARED
    SOURCES
        RenderModule.cpp
    PRIVATE_INC
    PUBLIC_INC
    LINK_LIBS
        SkyRenderAdaptor
        ImGuiRender
    )

sky_add_dependency(TARGET SkyRender DEPENDENCIES Launcher Editor)
