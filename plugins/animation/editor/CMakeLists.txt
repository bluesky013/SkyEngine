file(GLOB_RECURSE SRC_FILES src/* include/*)
sky_add_module(TARGET Animation.Editor
    SOURCES ${SRC_FILES} module/AnimationEditorModule.cpp
    PRIVATE_INC include
    LINK_LIBS Animation.GraphAssets.Static EditorCore EditorUi EditorWidgets Aurora.Renderer.Interface
)
sky_add_dependency(TARGET Animation.Editor DEPENDENCIES SandboxEditor)
if (SKY_BUILD_TEST)
    if (TARGET Animation.ModuleTest)
        target_link_libraries(Animation.ModuleTest EditorUi EditorWidgets)
        target_compile_definitions(Animation.ModuleTest PRIVATE ANIMATION_TEST_EDITOR=1)
        add_dependencies(Animation.ModuleTest Animation.Editor Aurora.Render.Editor Content.Views)
    endif ()
    sky_add_test(TARGET Animation.EditorTest
        SOURCES ${SRC_FILES} test/GraphDocumentTest.cpp test/GraphEditorTest.cpp test/main.cpp
        INCS include
        LIBS Animation.GraphAssets.Static EditorCore EditorUi EditorWidgets Aurora.Renderer.Interface 3rdParty::googletest
    )
endif ()
