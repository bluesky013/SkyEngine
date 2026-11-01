cmake_minimum_required(VERSION 3.20)
project(HarmonyOSHostAcceptance LANGUAGES C CXX)

if (NOT CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    message(FATAL_ERROR "Use the API 26 HarmonyOS native SDK toolchain")
endif ()
get_filename_component(ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include("${ENGINE_ROOT}/cmake/functions.cmake")
include("${ENGINE_ROOT}/cmake/options.cmake")
include("${ENGINE_ROOT}/cmake/configuration.cmake")
include("${ENGINE_ROOT}/cmake/plugin_switches.cmake")
include("${ENGINE_ROOT}/cmake/thirdparty_helpers.cmake")
set(sfmt_PATH "${3RD_PATH}/sfmt")
set(rapidjson_PATH "${3RD_PATH}/rapidjson")
include("${ENGINE_ROOT}/cmake/thirdparty/Findsfmt.cmake")
include("${ENGINE_ROOT}/cmake/thirdparty/Findrapidjson.cmake")
add_subdirectory("${ENGINE_ROOT}/engine/core" core)
add_subdirectory("${ENGINE_ROOT}/engine/framework" framework)

add_library(OHOSHostAcceptance SHARED HostAcceptance.cpp NapiBridge.cpp)
target_link_libraries(OHOSHostAcceptance PRIVATE Framework ace_napi.z ace_ndk.z)
target_link_options(OHOSHostAcceptance PRIVATE
    -Wl,--no-undefined
    -Wl,--wrap=OH_NativeVSync_Create
    -Wl,--wrap=OH_NativeVSync_RequestFrame
    -Wl,--wrap=OH_NativeVSync_Destroy
    -Wl,--wrap=OH_NativeWindow_NativeObjectReference
    -Wl,--wrap=OH_NativeWindow_NativeObjectUnreference)
