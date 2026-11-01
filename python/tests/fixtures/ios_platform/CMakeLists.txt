cmake_minimum_required(VERSION 3.20)
project(SkyIOSPlatformTests LANGUAGES C CXX OBJCXX)

if (NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(FATAL_ERROR "Use CMAKE_SYSTEM_NAME=iOS and an installed iOS SDK")
endif ()
get_filename_component(ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(SKY_BUILD_TEST OFF CACHE BOOL "Only build the UIKit platform test host" FORCE)
set(SKY_BUILD_TOOL OFF CACHE BOOL "No desktop tools in the UIKit test host" FORCE)
set(SKY_BUILD_SANDBOX OFF CACHE BOOL "No editor in the UIKit test host" FORCE)
set(SKY_USE_TRACY OFF CACHE BOOL "The platform test host does not use profiling" FORCE)
set(SKY_IOS_TEST_CONFIG Debug CACHE STRING "Configuration selected by the iOS test runner")
option(SKY_IOS_TEST_DEVICE "Sign only the UIKit app for a physical device" OFF)
option(SKY_IOS_TEST_MANUAL_INPUT "Keep the UIKit host in interactive input mode" OFF)
set(SKY_IOS_TEST_BUNDLE_ID "com.skyengine.platform-tests" CACHE STRING "UIKit test application identifier")
set(SKY_IOS_TEST_TEAM_ID "" CACHE STRING "Existing Apple development team for device tests")
include("${ENGINE_ROOT}/cmake/functions.cmake")
include("${ENGINE_ROOT}/cmake/options.cmake")
include("${ENGINE_ROOT}/cmake/plugin_switches.cmake")
include("${ENGINE_ROOT}/cmake/configuration.cmake")
list(APPEND CMAKE_MODULE_PATH "${ENGINE_ROOT}/cmake/thirdparty")
include("${ENGINE_ROOT}/cmake/thirdparty_helpers.cmake")

foreach (dependency sfmt rapidjson lz4)
    if (NOT IS_DIRECTORY "${3RD_PATH}/${dependency}/include")
        message(FATAL_ERROR "The simulator third-party root is missing ${dependency}/include; set 3RD_PATH")
    endif ()
    set(${dependency}_PATH "${3RD_PATH}/${dependency}")
    sky_find_3rd(TARGET ${dependency} DIR ${dependency})
endforeach ()
if (NOT EXISTS "${3RD_PATH}/sfmt/lib/${SKY_IOS_TEST_CONFIG}/${CMAKE_STATIC_LIBRARY_PREFIX}SFMT${CMAKE_STATIC_LIBRARY_SUFFIX}")
    message(FATAL_ERROR "Build SFMT for the selected iOS Simulator SDK, architecture and configuration")
endif ()
if (NOT EXISTS "${SKY_IOS_GTEST_ROOT}/CMakeLists.txt" OR NOT EXISTS "${SKY_IOS_GTEST_ROOT}/src/gtest-all.cc")
    message(FATAL_ERROR "Set SKY_IOS_GTEST_ROOT to the existing GoogleTest source directory; downloads are disabled")
endif ()
set(INSTALL_GTEST OFF CACHE BOOL "Do not install test dependencies" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "The platform tests use only GoogleTest" FORCE)
get_filename_component(gtest_checkout "${SKY_IOS_GTEST_ROOT}/.." ABSOLUTE)
if (NOT EXISTS "${gtest_checkout}/CMakeLists.txt" OR
    NOT EXISTS "${gtest_checkout}/googletest/src/gtest-all.cc")
    message(FATAL_ERROR "Set SKY_IOS_GTEST_ROOT to the googletest source directory inside its checkout")
endif ()
add_subdirectory("${gtest_checkout}" googletest EXCLUDE_FROM_ALL)
add_library(3rdParty::googletest ALIAS gtest)
add_subdirectory("${ENGINE_ROOT}/engine/core" core)
add_subdirectory("${ENGINE_ROOT}/engine/framework" framework)

set(test_root "${ENGINE_ROOT}/engine/framework/test/platform/ios")
set(test_sources "${test_root}/IOSPlatformStateTest.cpp" "${test_root}/IOSNativePlatformTest.mm" "${test_root}/main.mm")
set_source_files_properties("${test_root}/IOSNativePlatformTest.mm" "${test_root}/main.mm" PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
sky_add_test(TARGET IOSPlatformTest
    SOURCES ${test_sources}
    INCS "${ENGINE_ROOT}/engine/framework/platform/ios"
    LIBS Framework 3rdParty::googletest)
target_compile_definitions(IOSPlatformTest PRIVATE SKY_IOS_TEST_MANUAL_INPUT=$<BOOL:${SKY_IOS_TEST_MANUAL_INPUT}>)
set_target_properties(IOSPlatformTest PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${test_root}/Info.plist"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${SKY_IOS_TEST_BUNDLE_ID}"
    MACOSX_BUNDLE_BUNDLE_NAME "Sky Input Test"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
    OUTPUT_NAME "SkyIOSPlatformTests"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bundle")
if (SKY_IOS_TEST_DEVICE)
    if (NOT SKY_IOS_TEST_TEAM_ID)
        message(FATAL_ERROR "Physical UIKit tests require SKY_IOS_TEST_TEAM_ID")
    endif ()
    set_target_properties(IOSPlatformTest PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "YES"
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "YES"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${SKY_IOS_TEST_TEAM_ID}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${SKY_IOS_TEST_BUNDLE_ID}"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic")
else ()
    set_target_properties(IOSPlatformTest PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
endif ()
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/test-app-$<CONFIG>.txt" CONTENT "$<TARGET_BUNDLE_DIR:IOSPlatformTest>\n")
