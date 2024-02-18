file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

configure_file(ProjectRoot.h.in ${CMAKE_BINARY_DIR}/Gen/RHISample/ProjectRoot.h)
include_directories(${CMAKE_BINARY_DIR}/Gen)

sky_add_library(TARGET RHISample SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        RHI
        Framework
        ShaderCompiler.Static
    )
sky_add_dependency(TARGET RHISample DEPENDENCIES Launcher)