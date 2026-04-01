file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_find_3rd(TARGET gles DIR gles)

if (NOT ${gles_FOUND})
    message(WARNING "GLES SDK (PowerVR) not found, skipping AuroraGL")
    return()
endif()

sky_add_library(TARGET AuroraGL SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        Aurora.RHI
        3rdParty::gles
)

sky_add_dependency(TARGET AuroraGL DEPENDENCIES Launcher Editor AuroraTest)
