file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET BulletPhysicsModule SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        BulletModule.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Framework
        Physics
        RenderCore
        3rdParty::bullet3
)

sky_add_dependency(TARGET BulletPhysicsModule DEPENDENCIES Launcher Editor)