sky_add_library(TARGET Loader STATIC
    SOURCES
        Loader.cpp
        Loader.h
    PUBLIC_INC
        .
    )

add_subdirectory(gltf)
add_subdirectory(model)
add_subdirectory(shader)