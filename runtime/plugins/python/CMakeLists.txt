file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET PythonModule SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        PythonModule.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Framework
        3rdParty::cpython
)

sky_add_dependency(TARGET PythonModule DEPENDENCIES Launcher)