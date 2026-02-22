set(CURRENT_FOLDER Runtime/Render)

add_subdirectory(backend)
add_subdirectory(shader)
add_subdirectory(core)
add_subdirectory(adaptor)
add_subdirectory(imgui)
if ((SKY_BUILD_TOOL) AND ((${CMAKE_SYSTEM_NAME} STREQUAL "Darwin") OR (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")))
    add_subdirectory(builder)
    add_subdirectory(tool)
endif ()

sky_add_library(TARGET SkyRender SHARED
    SOURCES
        adaptor/RenderRegistry.cpp
    PRIVATE_INC
    PUBLIC_INC
    LINK_LIBS
        RenderAdaptor
        ImGuiRender
    )

if (SKY_BUILD_EDITOR)

    find_package(Qt5 COMPONENTS Widgets)

    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    file(GLOB_RECURSE EDITOR_SRC_FILES editor/src/*)
    file(GLOB_RECURSE EDITOR_INC_FILES editor/include/*)


    sky_add_library(TARGET SkyRender.Editor SHARED
    SOURCES
        editor/RenderEditorRegistry.cpp
        ${EDITOR_SRC_FILES}
        ${EDITOR_INC_FILES}
    PRIVATE_INC
        editor/src
    PUBLIC_INC
        editor/include
    LINK_LIBS
        RenderAdaptor
        ImGuiRender
        EditorFramework
        3rdParty::nodeeditor
    PUBLIC_DEFS
        NODE_EDITOR_STATIC
    )
endif ()

sky_add_dependency(TARGET SkyRender DEPENDENCIES Launcher)
sky_add_dependency(TARGET SkyRender.Editor DEPENDENCIES Editor)
