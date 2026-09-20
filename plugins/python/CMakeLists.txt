file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

include(${ENGINE_ROOT}/cmake/plugin_config.cmake)

sky_find_3rd(TARGET cpython DIR cpython)

sky_add_library(TARGET PythonModule SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        PythonModule.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Framework
        3rdParty::cpython
)

sky_plugin_apply_target_dependencies(CONFIG "${CMAKE_CURRENT_LIST_DIR}/plugin.json")

if (SKY_BUILD_TEST)
    sky_add_test(TARGET PythonRuntimeTest
        SOURCES
            ${CMAKE_CURRENT_LIST_DIR}/test/PythonRuntimeTest.cpp
        WORKING_DIR
            ${CMAKE_SOURCE_DIR}
        LIBS
            PythonModule
            Framework
            3rdParty::googletest
    )

    set_tests_properties(PythonRuntimeTest PROPERTIES
        ENVIRONMENT "SKY_PYTHON_HOME=${3RD_PATH}/cpython")
endif ()