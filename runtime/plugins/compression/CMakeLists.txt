file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET CompressionModule SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        Module.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Framework
        #3rdParty::lz4
)

sky_add_dependency(TARGET CompressionModule DEPENDENCIES Launcher Editor)