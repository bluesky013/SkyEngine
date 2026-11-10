file(GLOB_RECURSE SRC_FILES src/* include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET Curve.Assets.Static STATIC
    SOURCES
        ${SRC_FILES}
    PUBLIC_INC
        include
    PUBLIC_LINK_LIBS
        Curve
        Framework
)

sky_add_module(TARGET Curve.Assets
    SOURCES
        module/CurveAssetsModule.cpp
    LINK_LIBS
        Curve.Assets.Static
)
sky_add_dependency(TARGET Curve.Assets DEPENDENCIES Launcher SandboxEditor AssetTool)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Curve.AssetsTest
        SOURCES
            ${TEST_FILES}
        LIBS
            Curve.Assets.Static
            3rdParty::googletest
    )
endif ()
