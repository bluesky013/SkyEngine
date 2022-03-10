sky_add_exe(TARGET AssetTool ${PLATFORM_TYPE}
    SOURCES
        AssetTools.cpp
    )


add_dependencies(AssetTool
    ModelBuilder
    ShaderBuilder)