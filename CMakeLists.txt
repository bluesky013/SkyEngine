cmake_minimum_required(VERSION 3.10.0)

PROJECT(SkyEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ENGINE_ROOT ${CMAKE_CURRENT_LIST_DIR})

include(${ENGINE_ROOT}/cmake/functions.cmake)
include(${ENGINE_ROOT}/cmake/options.cmake)
include(${ENGINE_ROOT}/cmake/thirdparty.cmake)
include(${ENGINE_ROOT}/cmake/configuration.cmake)

if (NOT ANDROID)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${ENGINE_ROOT}/output/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ENGINE_ROOT}/output/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ENGINE_ROOT}/output/bin)
endif()

configure_file(EngineRoot.h.in ${CMAKE_BINARY_DIR}/Gen/EngineRoot.h)
include_directories(${CMAKE_BINARY_DIR}/Gen)

enable_testing()

add_subdirectory(launcher)
add_subdirectory(framework)
add_subdirectory(core)
add_subdirectory(engine)
add_subdirectory(backend)
add_subdirectory(builder)

if (NOT ANDROID)
    add_subdirectory(sample)
#    add_subdirectory(tools)
    add_subdirectory(test)
endif()
if (SKY_BUILD_EDITOR)
    add_subdirectory(editor)
endif()
