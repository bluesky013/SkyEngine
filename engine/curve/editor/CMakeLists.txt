file(GLOB_RECURSE SRC_FILES src/* include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET Curve.Editor.Static STATIC
    SOURCES
        ${SRC_FILES}
    PUBLIC_INC
        include
    PUBLIC_LINK_LIBS
        Curve
        Framework
        EditorCore
        EditorUi
    PRIVATE_LINK_LIBS
        Curve.Assets.Static
        EditorWidgets
)
set_target_properties(Curve.Editor.Static PROPERTIES POSITION_INDEPENDENT_CODE ON)

sky_add_module(TARGET Curve.Editor
    SOURCES
        module/CurveEditorModule.cpp
    PRIVATE_LINK_LIBS
        Curve.Editor.Static
)
sky_add_dependency(TARGET Curve.Editor DEPENDENCIES SandboxEditor)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Curve.EditorTest
        SOURCES
            ${TEST_FILES}
        LIBS
            Curve.Editor.Static
            EditorWidgets
            Curve.Assets.Static
            EditorWidgets
            3rdParty::googletest
    )
endif ()
