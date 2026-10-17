set(CURRENT_FOLDER Engine/UI)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

# Shared contracts and pure operations are a separate target within the UI
# foundation module. Runtime, editor and backend implementations stay in engine/ui.
sky_add_library(TARGET UI.Interface STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PUBLIC_INC
        include
)

# UI is an engine foundation, not an optional feature plugin.
add_subdirectory(core)
add_subdirectory(render)
if (SKY_BUILD_FREETYPE)
    add_subdirectory(freetype)
endif ()
if (SKY_BUILD_SANDBOX OR SKY_BUILD_EDITOR OR SKY_BUILD_TEST)
    add_subdirectory(editor)
endif ()
if (SKY_BUILD_TEST)
    add_subdirectory(test)
endif ()
