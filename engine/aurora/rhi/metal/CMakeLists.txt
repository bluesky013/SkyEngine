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
        ${METAL_EXT_LIBS}
)

sky_add_dependency(TARGET Aurora.Metal DEPENDENCIES Launcher Editor AuroraTest)
