file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET RecastModule SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        RecastModule.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Navigation
        3rdParty::recast
)

sky_add_dependency(TARGET RecastModule DEPENDENCIES Launcher Editor)