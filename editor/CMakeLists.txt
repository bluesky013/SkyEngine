find_package(Qt5 COMPONENTS Widgets)

if (Qt5_FOUND)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)

    set(QtLibs Qt5::Widgets)

    add_subdirectory(framework)

    file(GLOB_RECURSE EDITOR_SRC src/*)
    file(GLOB_RECURSE EDITOR_INC include/*)

    sky_add_exe(TARGET Editor
        SOURCES
            ${EDITOR_SRC}
            ${EDITOR_INC}
        INCS
            include
        LIBS
            SkyEngine
            SkyRenderAdaptor
            EditorFramework
        )

    if (WIN32)
        set(DeployTool windeployqt.exe)
        add_custom_command(TARGET Editor
            POST_BUILD
            COMMAND ${DeployTool} "$<TARGET_FILE:Editor>")
    endif ()
endif()

