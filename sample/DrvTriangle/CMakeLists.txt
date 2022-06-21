file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET DrvTriangleSample SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        TriangleModule.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Driver
        Framework
    )

add_dependencies(Launcher DrvTriangleSample)