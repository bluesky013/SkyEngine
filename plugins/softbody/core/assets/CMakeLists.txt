file(GLOB_RECURSE SOFTBODY_ASSET_SOURCES src/*.cpp include/*.h)
sky_add_library(TARGET SoftBody.Assets.Static STATIC
    SOURCES ${SOFTBODY_ASSET_SOURCES}
    PUBLIC_INC include
    LINK_LIBS Physics.SoftBody.Interface SoftBody.Authoring.Static Framework
)
sky_add_module(TARGET SoftBody.Assets
    SOURCES module/SoftBodyAssetsModule.cpp
    LINK_LIBS SoftBody.Assets.Static
)
sky_add_dependency(TARGET SoftBody.Assets DEPENDENCIES Launcher SandboxEditor AssetTool)
if (SKY_BUILD_TEST)
    file(GLOB SOFTBODY_ASSET_TESTS test/*.cpp)
    sky_add_test(TARGET SoftBody.AssetsTest SOURCES ${SOFTBODY_ASSET_TESTS}
        LIBS SoftBody.Assets.Static 3rdParty::googletest)
endif ()
