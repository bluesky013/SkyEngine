file(GLOB_RECURSE BUILDER_SRC src/*)
file(GLOB_RECURSE BUILDER_INC include/*)

sky_add_library(TARGET ShaderBuilder SHARED
    SOURCES
        ShaderModule.cpp
        ${BUILDER_SRC}
        ${BUILDER_INC}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Rapidjson
        SkyEngine
    )