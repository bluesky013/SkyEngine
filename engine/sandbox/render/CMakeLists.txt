set(CURRENT_FOLDER Sandbox/Render)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

# Editor-owned renderer: records its own passes on the Aurora device and
# presents to the editor swapchain (the GUI pipeline + UI pass). It is
# independent of the engine scene pipeline, and links Aurora + UI, so it is kept
# separate from the render-independent EditorCore.
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
        UIRender
        Aurora.RHI
        Aurora.Pipeline
        Aurora.Shader
)
