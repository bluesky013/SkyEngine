set(CURRENT_FOLDER Sandbox/Core)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

# Toolkit- and render-agnostic guard: EditorCore public headers must not depend
# on Aurora, the legacy render stack, the UI layer, or Qt. Panels are views over
# the core services, never the other way around.
file(GLOB_RECURSE EDITOR_CORE_PUBLIC_HEADERS include/editor/core/*.h)
foreach (header ${EDITOR_CORE_PUBLIC_HEADERS})
    file(READ ${header} EDITOR_CORE_HEADER_CONTENT)
    if (EDITOR_CORE_HEADER_CONTENT MATCHES "#include *[<\"]aurora/")
        message(FATAL_ERROR "EditorCore header must not include aurora: ${header}")
    endif ()
    if (EDITOR_CORE_HEADER_CONTENT MATCHES "#include *[<\"]render/")
        message(FATAL_ERROR "EditorCore header must not include render: ${header}")
    endif ()
    if (EDITOR_CORE_HEADER_CONTENT MATCHES "#include *[<\"]ui/")
        message(FATAL_ERROR "EditorCore header must not include ui: ${header}")
    endif ()
    if (EDITOR_CORE_HEADER_CONTENT MATCHES "#include *[<\"]Q")
        message(FATAL_ERROR "EditorCore header must not include Qt: ${header}")
    endif ()
endforeach ()

sky_add_library(TARGET EditorCore STATIC
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
        3rdParty::rapidjson
)
