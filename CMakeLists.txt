cmake_minimum_required(VERSION 3.10.0)

PROJECT(SkyEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/functions.cmake)

add_subdirectory(core)
add_subdirectory(vulkan)
add_subdirectory(sample)