sky_add_exe(TARGET AssetTool ${PLATFORM_TYPE}
    SOURCES
        AssetTools.cpp
    LIBS
        Framework
        Rapidjson
    )


add_dependencies(AssetTool
    ModelBuilder
    ShaderBuilder)