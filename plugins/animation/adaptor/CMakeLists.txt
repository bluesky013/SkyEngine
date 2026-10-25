file(GLOB_RECURSE SRC_FILES src/* include/*)
sky_add_library(TARGET Animation.World.Static STATIC
    SOURCES ${SRC_FILES}
    PUBLIC_INC include
    LINK_LIBS Animation.GraphAssets.Static Framework Aurora.Renderer.Interface
)
sky_add_module(TARGET Animation.World
    SOURCES module/AnimationWorldModule.cpp
    LINK_LIBS Animation.World.Static
)
sky_add_dependency(TARGET Animation.World DEPENDENCIES Launcher SandboxEditor)
if (SKY_BUILD_TEST)
    file(GLOB_RECURSE TEST_FILES test/*)
    sky_add_test(TARGET Animation.WorldTest
        SOURCES ${TEST_FILES}
        LIBS Animation.World.Static 3rdParty::googletest
    )
    sky_add_test(TARGET Animation.ModuleTest
        SOURCES module_test/AnimationModuleTest.cpp ../core/test/main.cpp
        LIBS Animation Framework Aurora.Renderer.Interface 3rdParty::googletest
    )
    add_dependencies(Animation.ModuleTest Animation.World Animation.GraphAssets Content.Module Content.Metadata Aurora.Render Aurora.Metadata)
endif ()
