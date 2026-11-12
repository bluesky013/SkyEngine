file(GLOB_RECURSE SOFTBODY_INTERFACE_SOURCES src/*.cpp include/*.h)
sky_add_library(TARGET Physics.SoftBody.Interface STATIC
    SOURCES ${SOFTBODY_INTERFACE_SOURCES}
    PUBLIC_INC include
    LINK_LIBS Physics
)
