set(CURRENT_FOLDER Engine/UI/Core)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

# The FreeType provider is only compiled when the freetype dependency is enabled.
if (NOT SKY_BUILD_FREETYPE)
    list(FILTER SRC_FILES EXCLUDE REGEX "/freetype/")
    list(FILTER INC_FILES EXCLUDE REGEX "/freetype/")
endif ()

# Render-agnostic guard: UI core public headers must not include render/aurora headers.
file(GLOB_RECURSE UI_PUBLIC_HEADERS include/ui/*.h)
foreach (header ${UI_PUBLIC_HEADERS})
    file(READ ${header} UI_HEADER_CONTENT)
    if (UI_HEADER_CONTENT MATCHES "#include *[<\"]aurora/")
        message(FATAL_ERROR "UI core header must not include aurora: ${header}")
    endif ()
    if (UI_HEADER_CONTENT MATCHES "#include *[<\"]render/")
        message(FATAL_ERROR "UI core header must not include render: ${header}")
    endif ()
endforeach ()

set(UI_LINK_LIBS
    Core
    Framework
    3rdParty::rapidjson
)

if (SKY_BUILD_FREETYPE)
    sky_find_3rd(TARGET freetype DIR freetype)
    list(APPEND UI_LINK_LIBS 3rdParty::freetype)
endif ()

sky_add_library(TARGET UI STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        ${UI_LINK_LIBS}
)
