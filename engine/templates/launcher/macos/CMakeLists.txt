# Project-owned macOS launcher. `SKY_MACOS_BUNDLE=ON` produces a .app.
option(SKY_MACOS_BUNDLE "build a macOS .app bundle" OFF)

if (SKY_MACOS_BUNDLE)
    sky_add_exe(TARGET Launcher MACOSX_BUNDLE
        SOURCES
            main.mm
        LIBS
            Framework
    )
    set_target_properties(Launcher PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "{{PROJECT_DISPLAY_NAME}}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${SKY_BUNDLE_ID}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
        XCODE_ATTRIBUTE_PRODUCT_NAME "{{PROJECT_NAME}}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${SKY_BUNDLE_ID}"
    )
else ()
    sky_add_exe(TARGET Launcher
        SOURCES
            main.mm
        LIBS
            Framework
    )
endif ()
set_target_properties(Launcher PROPERTIES
    OUTPUT_NAME "{{PROJECT_NAME}}"
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "@executable_path"
)

if (NOT SKY_APPLE_TEAM_ID STREQUAL "")
    set_target_properties(Launcher PROPERTIES XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${SKY_APPLE_TEAM_ID}")
endif ()
if (NOT SKY_APPLE_SIGNING_IDENTITY STREQUAL "")
    set_target_properties(Launcher PROPERTIES XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${SKY_APPLE_SIGNING_IDENTITY}")
endif ()
if (NOT SKY_APPLE_PROVISIONING_PROFILE STREQUAL "")
    set_target_properties(Launcher PROPERTIES XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER "${SKY_APPLE_PROVISIONING_PROFILE}")
endif ()
