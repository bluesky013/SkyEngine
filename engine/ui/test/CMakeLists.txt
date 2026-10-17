set(CURRENT_FOLDER Engine/UI/Test)

sky_add_test(TARGET UICoreTest
    SOURCES
        UIAnimationTest.cpp
        UICoreTest.cpp
        UIEventTest.cpp
        UIGeometryTest.cpp
        UIWidgetTest.cpp
        UIDataTest.cpp
        UIInputTest.cpp
        UIInputLifetimeTest.cpp
        UIInvalidationTest.cpp
        UILayoutTest.cpp
        UIMeasureTest.cpp
        UIPaintContextTest.cpp
        UIScrollTest.cpp
        UISystemTest.cpp
        UITransformTest.cpp
        main.cpp
    WORKING_DIR
        ${CMAKE_SOURCE_DIR}
    LIBS
        UI
        Core
        3rdParty::googletest
    )

# Embedded UI shader contract checks run without linking the UI runtime or renderer.
if (TARGET Aurora.ShaderCompiler.Static)
    sky_add_test(TARGET UIShaderTest
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/../render/test/UIShaderTest.cpp
            main.cpp
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../render/src
        WORKING_DIR
            ${CMAKE_SOURCE_DIR}
        LIBS
            Aurora.ShaderCompiler.Static
            Core
            3rdParty::googletest
    )
endif ()

sky_add_test(TARGET UITextTest
    SOURCES
        UITextTest.cpp
        main.cpp
    WORKING_DIR
        ${CMAKE_SOURCE_DIR}
    LIBS
        UI
        Core
        3rdParty::googletest
    )

sky_add_test(TARGET UIEditorTest
    SOURCES
        UIEditorTest.cpp
        main.cpp
    WORKING_DIR
        ${CMAKE_SOURCE_DIR}
    LIBS
        UI.Editor
        Core
        3rdParty::googletest
    )

# Concrete font backends are separate from the render-independent UI runtime.
if (SKY_BUILD_FREETYPE)
    sky_add_test(TARGET UITextFreeTypeTest
        SOURCES
            UITextFreeTypeTest.cpp
            main.cpp
        WORKING_DIR
            ${CMAKE_SOURCE_DIR}
        LIBS
            UI
            UI.FreeType
            Core
            3rdParty::googletest
        )
endif ()
