set(CURRENT_FOLDER Sandbox/Test)

sky_add_test(TARGET EditorCoreTest
    SOURCES
        CommandServiceTest.cpp
        PropertyModelTest.cpp
        DocumentServiceTest.cpp
        SelectionServiceTest.cpp
        LayoutModelTest.cpp
        LayoutPersistenceTest.cpp
        DockInteractionTest.cpp
        ShellModelsTest.cpp
        InputRouterTest.cpp
        LogServiceTest.cpp
        CommandControllerTest.cpp
        DefaultPanelsTest.cpp
        EditorExtensionTest.cpp
        ViewportTest.cpp
        ViewportControlsTest.cpp
        FileBrowserModelTest.cpp
        TextEditStateTest.cpp
        EditorSettingsTest.cpp
        BuiltinPreferencePagesTest.cpp
        FilePreferenceStorageTest.cpp
        PreferencesPersistenceTest.cpp
        PreferencesApplyResultTest.cpp
        PreferencesCopyLifetimeTest.cpp
        WorldDocumentTest.cpp
        PlaySessionTest.cpp
        ReflectedFormTest.cpp
        EditorAssetCatalogTest.cpp
        AssetSettingsHistoryTest.cpp
        main.cpp
    WORKING_DIR
        ${CMAKE_SOURCE_DIR}
    LIBS
        EditorCore
        Framework
        Core
        3rdParty::googletest
)

# Headless shell-level test: view registry and gesture hit-testing math.
if (SKY_BUILD_SANDBOX)
sky_add_test(TARGET EditorRenderTest
    SOURCES
        EditorWindowEventBindingTest.cpp
        EditorFrameSubmissionTest.cpp
        CpuViewportPickerTest.cpp
        main.cpp
    INCS
        ${CMAKE_CURRENT_SOURCE_DIR}/../render/src
    WORKING_DIR
        ${CMAKE_SOURCE_DIR}
    LIBS
        Framework
        Core
        Aurora.RHI.Static
        Aurora.Renderer.Interface
        EditorRender
        3rdParty::googletest
)

sky_add_test(TARGET EditorShellTest
    SOURCES
        EditorShellTest.cpp
        SceneViewportTest.cpp
        EditorLayoutDependencyTest.cpp
        EditorHistoryRoutingTest.cpp
        PreferencesSessionTest.cpp
        PreferencesLayoutTest.cpp
        PreferencesPersistenceShellTest.cpp
        EditorActionStateTest.cpp
        EditorPortableIntegrationTest.cpp
        EditorPreferenceIntegrationTest.cpp
        EditorSmallPanelLayoutTest.cpp
        FileBrowserLayoutTest.cpp
        ProjectManagerLayoutTest.cpp
        main.cpp
    WORKING_DIR
        ${CMAKE_SOURCE_DIR}
    LIBS
        EditorShell
        UI
        EditorCore
        Core
        3rdParty::googletest
)
endif ()

if (SKY_BUILD_SANDBOX AND SKY_BUILD_TEST)
    # Two separately linked modules exercise process-shared editor services
    # across a real DLL boundary, independently of the editor shell.
    foreach (probe EditorWidgetsProbeA EditorWidgetsProbeB)
        sky_add_library(TARGET ${probe} SHARED
            SOURCES
                EditorWidgetsProbe.cpp
            LINK_LIBS
                EditorWidgets
                Core
        )
    endforeach ()

    sky_add_test(TARGET EditorWidgetsTest
        SOURCES
            EditorWidgetsTest.cpp
            EditorScrollbarTest.cpp
            ReflectedFormWidgetsTest.cpp
            main.cpp
        WORKING_DIR
            ${CMAKE_SOURCE_DIR}
        LIBS
            EditorWidgets
            3rdParty::googletest
    )
    target_compile_definitions(EditorWidgetsTest PRIVATE
        "EDITOR_WIDGETS_PROBE_A_PATH=\"$<TARGET_FILE:EditorWidgetsProbeA>\""
        "EDITOR_WIDGETS_PROBE_B_PATH=\"$<TARGET_FILE:EditorWidgetsProbeB>\""
    )
    add_dependencies(EditorWidgetsTest EditorWidgetsProbeA EditorWidgetsProbeB)
endif ()

if (SKY_BUILD_SANDBOX AND SKY_BUILD_TEST)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    sky_add_test(TARGET SandboxProjectTest
        SOURCES
            SandboxProjectTest.cpp
            WorldEditorStateTest.cpp
            ../module/src/SandboxModule.cpp
            ../module/src/WorldEditorState.cpp
            ../module/src/UiIconBuilder.cpp
            main.cpp
        WORKING_DIR ${CMAKE_SOURCE_DIR}
        LIBS EditorRender EditorShell EditorCore Framework Core 3rdParty::nanosvg 3rdParty::googletest
    )
    target_include_directories(SandboxProjectTest PRIVATE ../module/include ../module/src)
    target_compile_definitions(SandboxProjectTest PRIVATE
        SKY_TEST_ENGINE_ROOT="${CMAKE_SOURCE_DIR}"
        SKY_TEST_PYTHON="${Python3_EXECUTABLE}"
    )
endif ()
