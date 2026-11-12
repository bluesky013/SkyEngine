file(GLOB_RECURSE ASSET_HEADERS include/*)
sky_add_library(TARGET Animation.GraphAssets.Static STATIC
    SOURCES
        src/AnimationComponentData.cpp
        src/GraphAsset.cpp
        src/GraphSource.cpp
        ${ASSET_HEADERS}
    PUBLIC_INC include
    LINK_LIBS Animation Framework
)
sky_add_module(TARGET Animation.GraphAssets
    SOURCES module/GraphAssetsModule.cpp
    LINK_LIBS Animation.GraphAssets.Static
)
sky_add_dependency(TARGET Animation.GraphAssets DEPENDENCIES Launcher SandboxEditor AssetTool)
