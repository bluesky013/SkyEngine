sky_add_library(TARGET Loader STATIC
    SOURCES
        Loader.cpp
        Loader.h
    PUBLIC_INC
        .
    )

add_subdirectory(gltf)