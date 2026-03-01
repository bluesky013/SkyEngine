file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

if (SKY_BUILD_XR)
    set(RHI_EXT_LIB 3rdParty::OpenXR)
endif ()

sky_add_library(TARGET RHI STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        ${PLATFORM_SRC}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        ${RHI_EXT_LIB}
    INSTALL_DIR
        include
    )

if (SKY_BUILD_XR)
    target_compile_definitions(RHI PUBLIC -DSKY_ENABLE_XR)
endif ()