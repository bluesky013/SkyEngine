set(CURRENT_FOLDER Engine/UI/Editor)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

# Headless document editing model. Visual tools use the Sandbox editor
# framework; the UI runtime never depends on this editor target.
sky_add_library(TARGET UI.Editor STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        UI
        3rdParty::rapidjson
)
