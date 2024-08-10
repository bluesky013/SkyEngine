add_subdirectory(backend)
add_subdirectory(shader)
add_subdirectory(core)
add_subdirectory(adaptor)
add_subdirectory(imgui)
add_subdirectory(animation)
if ((SKY_BUILD_TOOL) AND ((${CMAKE_SYSTEM_NAME} STREQUAL "Darwin") OR (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")))
    add_subdirectory(builder)
endif ()

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
