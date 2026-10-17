file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

if (NOT APPLE)
    message(WARNING "Metal is only supported on Apple platforms, skipping Aurora.Metal")
    return()
endif()

set(METAL_EXT_LIBS
    "-framework Foundation"
    "-framework QuartzCore"
    "-framework Metal"
)

# The backend uses ARC ownership idioms (__bridge_retained / __bridge_transfer
# between native handles and void* members); without ARC those casts are no-ops
# and leak. Scoped to the Obj-C++ sources only (must be set before the target).
file(GLOB_RECURSE METAL_OBJCXX_SRC src/*.mm)
set_source_files_properties(${METAL_OBJCXX_SRC} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")

sky_add_library(TARGET Aurora.Metal SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        Aurora.RHI.Static
        Aurora.FrameBackend.Static
        ${METAL_EXT_LIBS}
)

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # iOS accepts embedded frameworks, not standalone third-party dylibs.
    set_target_properties(Aurora.Metal PROPERTIES
        FRAMEWORK TRUE
        MACOSX_FRAMEWORK_IDENTIFIER "com.skyengine.aurora.metal"
        MACOSX_FRAMEWORK_BUNDLE_NAME "Aurora.Metal"
        MACOSX_FRAMEWORK_BUNDLE_VERSION "1"
        MACOSX_FRAMEWORK_SHORT_VERSION_STRING "1.0"
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_NAME_DIR "@rpath"
        INSTALL_RPATH "@loader_path/.."
    )
endif ()

sky_add_dependency(TARGET Aurora.Metal DEPENDENCIES Launcher Editor AuroraTest SandboxEditor)
