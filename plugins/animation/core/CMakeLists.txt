file(GLOB_RECURSE CORE_SOURCES src/* include/animation/graph/*)
sky_add_library(TARGET Animation.Graph.Static STATIC
    SOURCES ${CORE_SOURCES}
    PUBLIC_INC include
    LINK_LIBS Animation
)
file(GLOB_RECURSE ASSET_SOURCES assets/* include/animation/assets/*)
sky_add_library(TARGET Animation.GraphAssets.Static STATIC
    SOURCES ${ASSET_SOURCES}
    PUBLIC_INC include
    LINK_LIBS Animation.Graph.Static Framework
)
sky_add_module(TARGET Animation.GraphAssets
    SOURCES module/GraphAssetsModule.cpp
    LINK_LIBS Animation.GraphAssets.Static
)
sky_add_dependency(TARGET Animation.GraphAssets DEPENDENCIES Launcher SandboxEditor AssetTool)
if (SKY_BUILD_TEST)
    file(GLOB_RECURSE TEST_SOURCES test/*)
    sky_add_test(TARGET Animation.GraphTest
        SOURCES ${TEST_SOURCES}
        LIBS Animation.Graph.Static 3rdParty::googletest
    )
endif ()
