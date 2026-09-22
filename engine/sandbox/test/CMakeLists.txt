set(CURRENT_FOLDER Sandbox/Test)

sky_add_test(TARGET EditorCoreTest
    SOURCES
        CommandServiceTest.cpp
        PropertyModelTest.cpp
        DocumentServiceTest.cpp
        SelectionServiceTest.cpp
        LayoutModelTest.cpp
        LayoutPersistenceTest.cpp
        InputRouterTest.cpp
        LogServiceTest.cpp
        CommandControllerTest.cpp
        DefaultPanelsTest.cpp
        EditorExtensionTest.cpp
        ViewportTest.cpp
        main.cpp
    WORKING_DIR
        ${CMAKE_SOURCE_DIR}
    LIBS
        EditorCore
        Framework
        Core
        3rdParty::googletest
)
