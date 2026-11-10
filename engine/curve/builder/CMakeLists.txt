file(GLOB_RECURSE SRC_FILES src/* include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET Curve.Builder.Static STATIC
    SOURCES
        ${SRC_FILES}
    PUBLIC_INC
        include
    PRIVATE_LINK_LIBS
        Curve
        Curve.Assets.Static
        Framework
)

sky_add_module(TARGET Curve.Builder
    SOURCES
        module/CurveBuilderModule.cpp
    PRIVATE_LINK_LIBS
        Curve.Builder.Static
        Framework
)
sky_add_dependency(TARGET Curve.Builder DEPENDENCIES AssetTool SandboxEditor)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Curve.BuilderTest
        SOURCES
            ${TEST_FILES}
        LIBS
            Curve.Builder.Static
            Curve.Assets.Static
            3rdParty::googletest
    )
endif ()
