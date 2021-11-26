file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET Sample STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        SkyEngine
    )

sky_add_library(TARGET SampleModule SHARED
    SOURCES
        SampleModule.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Sample
    )