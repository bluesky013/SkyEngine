cmake_minimum_required(VERSION 3.10.0)

PROJECT(SkyEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ENGINE_ROOT ${CMAKE_CURRENT_LIST_DIR})

include(${ENGINE_ROOT}/cmake/functions.cmake)
include(${ENGINE_ROOT}/cmake/options.cmake)
include(${ENGINE_ROOT}/cmake/configuration.cmake)
include(${ENGINE_ROOT}/cmake/thirdparty.cmake)

if (NOT ANDROID)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${ENGINE_ROOT}/output/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ENGINE_ROOT}/output/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ENGINE_ROOT}/output/bin)
endif()

if (SKY_BUILD_TEST)
    enable_testing()
    add_subdirectory(test)
endif()

add_subdirectory(runtime)
add_subdirectory(launcher)

if (SKY_BUILD_EDITOR)
    add_subdirectory(editor)
    sky_set_dependency(TARGET Editor)
endif()

if (SKY_BUILD_TOOL)
    add_subdirectory(tools)
endif()

sky_set_dependency(TARGET Launcher)