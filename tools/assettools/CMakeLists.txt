file(GLOB_RECURSE ASSET_TOOL_SRC src/*)
file(GLOB_RECURSE ASSET_TOOL_INC include/*)

sky_add_library(TARGET AssetToolModule SHARED
    SOURCES
        ${ASSET_TOOL_SRC}
        ${ASSET_TOOL_INC}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Framework
    )


sky_add_exe(TARGET AssetTool ${PLATFORM_TYPE}
    SOURCES
        AssetTools.cpp
    LIBS
        Framework
        Rapidjson
    )


add_dependencies(AssetTool
    ModelBuilder
    ShaderBuilder
    AssetToolModule)