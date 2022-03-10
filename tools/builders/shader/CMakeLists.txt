file(GLOB_RECURSE LOADER_SRC src/*)
file(GLOB_RECURSE LOADER_INC include/*)

sky_add_library(TARGET ShaderBuilder SHARED
    SOURCES
        ${LOADER_SRC}
        ${LOADER_INC}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Rapidjson
        SkyEngine
    )