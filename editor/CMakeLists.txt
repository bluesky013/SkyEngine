find_package(Qt6 COMPONENTS Widgets
    PATHS ${QT_PATH})

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(QtLibs
    Qt6::Widgets
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
        ${QtLibs}
    )

add_dependencies(Launcher SkyEngineModule SampleModule)

if (WIN32)
    set(DeployTool ${QT_PATH}/bin/windeployqt.exe)
    add_custom_command(TARGET Editor
        POST_BUILD
        COMMAND ${DeployTool} "$<TARGET_FILE:Editor>")
endif ()