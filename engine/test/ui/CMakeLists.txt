sky_add_test(TARGET UICoreTest
    SOURCES
        UIAnimationTest.cpp
        UICoreTest.cpp
        UIEventTest.cpp
        UIWidgetTest.cpp
        UIDataTest.cpp
        UIEditorTest.cpp
        UIInputTest.cpp
        UILayoutTest.cpp
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

# The FreeType provider is part of the UI module when SKY_BUILD_FREETYPE is on.
if (SKY_BUILD_FREETYPE)
    sky_add_test(TARGET UITextFreeTypeTest
        SOURCES
            UITextFreeTypeTest.cpp
            main.cpp
        WORKING_DIR
            ${CMAKE_SOURCE_DIR}
        LIBS
            UI
            Core
            3rdParty::googletest
        )
endif ()
