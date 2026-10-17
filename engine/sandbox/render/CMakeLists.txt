set(CURRENT_FOLDER Sandbox/Render)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

set(EDITOR_UI_FONT_LIBS)
if (SKY_BUILD_FREETYPE)
    list(APPEND EDITOR_UI_FONT_LIBS UI.FreeType)
endif ()

set(EDITOR_RENDER_LIBS Aurora.Adaptor.Static Aurora.ShaderCompiler.Static)
if (NOT WIN32)
    list(APPEND EDITOR_RENDER_LIBS UIRender Aurora.RHI.Static Aurora.Pipeline.Static)
endif ()
sky_add_library(TARGET EditorRender STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        Framework
        EditorCore
        UI
        ${EDITOR_UI_FONT_LIBS}
        Aurora.Renderer.Interface
        ${EDITOR_RENDER_LIBS}
)
