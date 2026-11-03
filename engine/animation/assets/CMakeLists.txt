file(GLOB_RECURSE ASSET_SOURCES src/* include/*)
sky_add_library(TARGET Animation.GraphAssets.Static STATIC
    SOURCES ${ASSET_SOURCES}
    PUBLIC_INC include
    LINK_LIBS Animation Framework
)
sky_add_module(TARGET Animation.GraphAssets
    SOURCES module/GraphAssetsModule.cpp
    LINK_LIBS Animation.GraphAssets.Static
)
sky_add_dependency(TARGET Animation.GraphAssets DEPENDENCIES Launcher SandboxEditor AssetTool)
