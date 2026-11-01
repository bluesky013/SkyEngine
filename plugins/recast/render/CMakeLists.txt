file(GLOB_RECURSE RENDER_SOURCES src/*.cpp include/*.h)
sky_add_library(TARGET RecastNavigation.Render.Static STATIC
    SOURCES ${RENDER_SOURCES}
    PUBLIC_INC include
    LINK_LIBS Navigation Aurora.Adaptor.Static
)
set_target_properties(RecastNavigation.Render.Static PROPERTIES POSITION_INDEPENDENT_CODE ON)
if (SKY_BUILD_TEST)
    sky_add_test(TARGET RecastNavigation.RenderTest
        SOURCES test/NavigationRenderBridgeTest.cpp test/main.cpp
        LIBS RecastNavigation.Render.Static 3rdParty::googletest
    )
endif ()
