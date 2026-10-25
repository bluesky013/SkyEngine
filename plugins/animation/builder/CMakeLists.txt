file(GLOB_RECURSE SOURCES src/* include/*)
sky_add_library(TARGET Animation.Builder.Static STATIC
    SOURCES ${SOURCES}
    PUBLIC_INC include
    LINK_LIBS Animation.GraphAssets.Static Framework
)
sky_add_module(TARGET Animation.Builder
    SOURCES module/AnimationBuilderModule.cpp
    LINK_LIBS Animation.Builder.Static
)
sky_add_dependency(TARGET Animation.Builder DEPENDENCIES AssetTool SandboxEditor)
if (TARGET Animation.ModuleTest)
    add_dependencies(Animation.ModuleTest Animation.Builder Aurora.Cook Content.Builder.Module)
    target_compile_definitions(Animation.ModuleTest PRIVATE ANIMATION_TEST_COOK=1)
endif ()
if (SKY_BUILD_TEST)
    file(GLOB_RECURSE TEST_FILES test/*)
    sky_add_test(TARGET Animation.BuilderTest
        SOURCES ${TEST_FILES}
        LIBS Animation.Builder.Static 3rdParty::googletest
    )
endif ()
