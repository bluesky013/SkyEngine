file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

# Aurora editor extension module (loaded by the editor through module config,
# mirrors legacy SkyRender.Editor). Links the bridge for AuroraModule.
sky_add_library(TARGET AuroraRender.Editor SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PUBLIC_INC
        include
    LINK_LIBS
        Aurora.Adaptor
        Core
)
