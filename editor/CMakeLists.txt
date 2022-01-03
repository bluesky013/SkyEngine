set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED
    PATHS ${QT_PATH})

set(Qt5Libs
    Qt5::Widgets
)

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
        ${Qt5Libs}
    )

add_dependencies(Launcher SkyEngineModule SampleModule)

if (WIN32)
    set(DeployTool ${QT_PATH}/bin/windeployqt.exe)
    add_custom_command(TARGET Editor
        POST_BUILD
        COMMAND ${DeployTool} "$<TARGET_FILE:Editor>")
endif ()