file(GLOB_RECURSE EDITOR_SOURCES src/*.cpp include/*.h)
sky_add_library(TARGET RecastNavigation.Editor.Static STATIC
    SOURCES ${EDITOR_SOURCES}
    PUBLIC_INC include
    LINK_LIBS Navigation RecastNavigation.Render.Static EditorCore EditorRender
)
set_target_properties(RecastNavigation.Editor.Static PROPERTIES POSITION_INDEPENDENT_CODE ON)
sky_add_module(TARGET RecastNavigation.Editor
    SOURCES module/NavigationEditorModule.cpp
    LINK_LIBS RecastNavigation.Editor.Static
)
sky_add_dependency(TARGET RecastNavigation.Editor DEPENDENCIES SandboxEditor)
if (SKY_BUILD_TEST)
    sky_add_test(TARGET RecastNavigation.EditorTest
        SOURCES test/NavigationEditorTest.cpp test/main.cpp
        LIBS RecastNavigation.Editor.Static RecastNavigation.Runtime.Static 3rdParty::googletest
    )
endif ()
