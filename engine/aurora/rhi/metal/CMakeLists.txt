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