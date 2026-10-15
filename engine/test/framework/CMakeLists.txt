file(GLOB TEST_SRC *.cpp *.h)
file(GLOB PLATFORM_POLICY_TEST_SRC platform/android/*.cpp platform/ios/*.cpp)
list(APPEND TEST_SRC ${PLATFORM_POLICY_TEST_SRC})

if (ANDROID)
    # The headless Android test entry also supplies the official native glue hook.
    list(REMOVE_ITEM TEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    file(GLOB NATIVE_PLATFORM_TEST_SRC platform/macos/*.mm)
    list(APPEND TEST_SRC ${NATIVE_PLATFORM_TEST_SRC})
elseif (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # UIKit integration tests must run inside UIApplicationMain on its main thread.
    list(REMOVE_ITEM TEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
    file(GLOB NATIVE_PLATFORM_TEST_SRC platform/ios/*.mm)
    set_source_files_properties(${NATIVE_PLATFORM_TEST_SRC} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    list(APPEND TEST_SRC ${NATIVE_PLATFORM_TEST_SRC})
endif ()

# Android input mapping is SDK-independent and is exercised on every host.
# The native Android Framework already includes it through its platform glob.
if (NOT ANDROID)
    list(APPEND TEST_SRC ${ENGINE_ROOT}/engine/framework/platform/android/AndroidInput.cpp)
endif ()

sky_add_test(TARGET FrameworkTest
    SOURCES
        ${TEST_SRC}
    WORKING_DIR
        ${CMAKE_SOURCE_DIR}
    LIBS
        Framework
        3rdParty::rapidjson
        3rdParty::googletest
    )

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set_target_properties(FrameworkTest PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/platform/ios/Info.plist"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.skyengine.FrameworkTest"
        MACOSX_BUNDLE_BUNDLE_NAME "FrameworkTest"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1")
endif ()

# The export preset schema manifest (single source of truth); the test reads it
# by absolute path so it does not depend on the working directory.
target_compile_definitions(FrameworkTest PRIVATE
    SKY_EXPORT_PRESET_SCHEMA="${CMAKE_SOURCE_DIR}/configs/export_preset.schema.json")

target_include_directories(FrameworkTest PRIVATE
    ${ENGINE_ROOT}/engine/framework/platform/android
    ${ENGINE_ROOT}/engine/framework/platform/ios)
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_include_directories(FrameworkTest PRIVATE ${ENGINE_ROOT}/engine/framework/platform/macos)
endif ()
