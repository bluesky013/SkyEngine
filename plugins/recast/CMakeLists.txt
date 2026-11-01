include(${ENGINE_ROOT}/cmake/plugin_config.cmake)
sky_find_3rd(TARGET recast DIR recast)

file(GLOB_RECURSE SHARED_SOURCES shared/src/*.cpp shared/include/*.h)
sky_add_library(TARGET RecastNavigation.Shared.Static STATIC
    SOURCES ${SHARED_SOURCES} src/NavigationSubSystemConfig.cpp include/recast/NavigationSubSystemConfig.h
    PUBLIC_INC shared/include include
    LINK_LIBS Navigation Framework
)
set_target_properties(RecastNavigation.Shared.Static PROPERTIES POSITION_INDEPENDENT_CODE ON)

file(GLOB_RECURSE RUNTIME_SOURCES runtime/src/*.cpp runtime/include/*.h)
sky_add_library(TARGET RecastNavigation.Runtime.Static STATIC
    SOURCES ${RUNTIME_SOURCES}
    PUBLIC_INC runtime/include
    LINK_LIBS Navigation Framework RecastNavigation.Shared.Static
)
set_target_properties(RecastNavigation.Runtime.Static PROPERTIES POSITION_INDEPENDENT_CODE ON)

file(GLOB_RECURSE BACKEND_SOURCES src/*.cpp include/*.h)
list(REMOVE_ITEM BACKEND_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NavigationSubSystemConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/recast/NavigationSubSystemConfig.h)
sky_add_library(TARGET RecastNavigation.Static STATIC
    SOURCES ${BACKEND_SOURCES}
    PRIVATE_INC src
    PUBLIC_INC include
    LINK_LIBS Navigation Physics RecastNavigation.Shared.Static 3rdParty::recast
)
set_target_properties(RecastNavigation.Static PROPERTIES POSITION_INDEPENDENT_CODE ON)

sky_add_module(TARGET RecastNavigation
    SOURCES RecastModule.cpp
    LINK_LIBS RecastNavigation.Static RecastNavigation.Runtime.Static
)

if ((SKY_DEVELOP OR SKY_BUILD_TEST) AND NOT SKY_MOBILE)
    add_subdirectory(builder)
endif ()
add_subdirectory(render)
if (TARGET EditorCore AND TARGET EditorRender)
    add_subdirectory(editor)
endif ()
sky_plugin_apply_target_dependencies(CONFIG "${CMAKE_CURRENT_LIST_DIR}/plugin.json")

if (SKY_BUILD_TEST)
    file(GLOB_RECURSE RUNTIME_TEST_SOURCES runtime/test/*.cpp shared/test/*.cpp)
    sky_add_test(TARGET NavigationTest
        SOURCES ${RUNTIME_TEST_SOURCES} ${ENGINE_ROOT}/engine/navigation/test/main.cpp
        LIBS RecastNavigation.Runtime.Static RecastNavigation.Shared.Static 3rdParty::googletest
        WORKING_DIR $<TARGET_FILE_DIR:NavigationTest>)
    sky_add_test(TARGET RecastModuleTest
        SOURCES test/main.cpp test/RecastModuleLoadTest.cpp test/RecastSceneTest.cpp
        LIBS Navigation RecastNavigation.Shared.Static 3rdParty::googletest
        WORKING_DIR $<TARGET_FILE_DIR:RecastNavigation>)
    add_dependencies(RecastModuleTest RecastNavigation)
    sky_add_test(TARGET RecastNavigationTest
        SOURCES test/main.cpp test/RecastNavigationTest.cpp
        LIBS RecastNavigation.Static RecastNavigation.Runtime.Static 3rdParty::googletest
        WORKING_DIR $<TARGET_FILE_DIR:RecastNavigationTest>)
endif ()
