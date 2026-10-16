set(CURRENT_FOLDER Engine/UI/Core)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

# Public runtime headers must remain independent of concrete render, font and
# editor backends. Hosts explicitly opt into those separate adaptor targets.
file(GLOB_RECURSE UI_PUBLIC_HEADERS include/ui/*.h)
foreach (header ${UI_PUBLIC_HEADERS})
    file(READ ${header} UI_HEADER_CONTENT)
    if (UI_HEADER_CONTENT MATCHES "#include *[<\"](aurora/|render/|ui/render/|ui/freetype/|ui/editor/)")
        message(FATAL_ERROR "UI core header must not include a backend or editor: ${header}")
    endif ()
endforeach ()

sky_add_library(TARGET UI STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        UI.Interface
        Core
        3rdParty::rapidjson
)
