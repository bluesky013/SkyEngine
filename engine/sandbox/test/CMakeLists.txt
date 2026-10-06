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
        FileBrowserModelTest.cpp
        TextEditStateTest.cpp
        PreferenceStoreTest.cpp
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
sky_add_test(TARGET EditorShellTest
    SOURCES
        EditorShellTest.cpp
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
