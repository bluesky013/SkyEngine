file(GLOB_RECURSE SOFTBODY_AUTHORING_SOURCES src/*.cpp src/*.h include/*.h)
sky_add_library(TARGET SoftBody.Authoring.Static STATIC
    SOURCES ${SOFTBODY_AUTHORING_SOURCES}
    PUBLIC_INC include
    LINK_LIBS Physics.SoftBody.Interface
)
if (SKY_BUILD_TEST)
    file(GLOB SOFTBODY_AUTHORING_TESTS test/*.cpp)
    sky_add_test(TARGET SoftBody.AuthoringTest SOURCES ${SOFTBODY_AUTHORING_TESTS}
        LIBS SoftBody.Authoring.Static 3rdParty::googletest)
endif ()
