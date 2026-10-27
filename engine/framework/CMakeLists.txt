set(CURRENT_FOLDER Engine/Foundation)

file(GLOB_RECURSE APP_SRC src/*)
file(GLOB_RECURSE APP_INC include/*)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    file(GLOB_RECURSE PLATFORM_SRC
        platform/windows/*)

    set(FM_EXT_LIB
        )
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
    # native UIKit window/event backend. Note: an iOS toolchain sets
    # CMAKE_SYSTEM_NAME to "iOS", not "Darwin", so this branch is required and
    # must not fall through to the macOS (Cocoa) sources.
    file(GLOB_RECURSE PLATFORM_SRC
        platform/ios/*
        platform/posix/*)
    set(FM_EXT_LIB
        "-framework UIKit"
        "-framework QuartzCore"
        "-framework Metal"
        "-framework Foundation")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    # native Cocoa window/event backend (no SDL)
    file(GLOB_RECURSE PLATFORM_SRC
        platform/macos/*
        platform/posix/*)
    set(FM_EXT_LIB
        "-framework Cocoa"
        "-framework QuartzCore")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    find_package(game-activity REQUIRED CONFIG)
    if (NOT TARGET game-activity::game-activity_static)
        message(FATAL_ERROR "Android requires GameActivity 1.2.2 or newer with its Prefab static library")
    endif ()

    file(GLOB_RECURSE PLATFORM_SRC
        platform/android/*)
    set(FM_EXT_LIB
        log
        android
        game-activity::game-activity_static)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    # Native X11 window/event backend (no SDL) + shared POSIX process backend.
    file(GLOB_RECURSE PLATFORM_SRC
        platform/linux/*
        platform/posix/*)
    set(FM_EXT_LIB
        X11
        ${CMAKE_DL_LIBS})
endif ()

# The UIKit backend uses ARC ownership idioms (__bridge_retained/transfer for
# native handles). Must be set before the target is created.
if (${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
    file(GLOB_RECURSE IOS_OBJCXX_SRC platform/ios/*.mm)
    set_source_files_properties(${IOS_OBJCXX_SRC} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endif ()

if (ANDROID OR SKY_BUILD_TEST)
    sky_add_library(TARGET Framework.AndroidInput STATIC
        SOURCES platform/android/AndroidInput.cpp
        PRIVATE_INC include
        LINK_LIBS Core
    )
    set_target_properties(Framework.AndroidInput PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif ()
if (ANDROID)
    list(REMOVE_ITEM PLATFORM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/platform/android/AndroidInput.cpp)
    list(APPEND FM_EXT_LIB Framework.AndroidInput)
    install(TARGETS Framework.AndroidInput EXPORT SkyEngineExport ARCHIVE DESTINATION lib)
endif ()

sky_add_library(TARGET Framework STATIC
    SOURCES
        ${PLATFORM_SRC}
        ${APP_SRC}
        ${APP_INC}
    PRIVATE_INC
        src
        "${CMAKE_BINARY_DIR}/generated/framework"
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        ${FM_EXT_LIB}
        ${PLATFORM_EXT_LIBS}
    INSTALL_DIR
        include
    )

if (SKY_BUILD_TEST)
    add_subdirectory(test)
endif ()
