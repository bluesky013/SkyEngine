file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

set(METAL_EXT_LIBS
    "-framework Foundation"
    "-framework QuartzCore"
    "-framework Metal"
    )

sky_add_library(TARGET MetalRHI SHARED
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
        RHI
        3rdParty::metal-cpp
        ${PLATFORM_EXT_LIBS}
        ${METAL_EXT_LIBS}
    )
add_dependencies(Launcher MetalRHI)
