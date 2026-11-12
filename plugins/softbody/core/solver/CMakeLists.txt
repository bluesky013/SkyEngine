file(GLOB_RECURSE SOFTBODY_SOLVER_SOURCES src/*.cpp include/*.h)
sky_add_library(TARGET SoftBody.Solver.Static STATIC
    SOURCES ${SOFTBODY_SOLVER_SOURCES}
    PUBLIC_INC include
    LINK_LIBS Core Physics.SoftBody.Interface
)
if (SKY_BUILD_TEST)
    file(GLOB SOFTBODY_SOLVER_TESTS test/*.cpp)
    sky_add_test(TARGET SoftBody.SolverTest SOURCES ${SOFTBODY_SOLVER_TESTS}
        LIBS SoftBody.Solver.Static 3rdParty::googletest)
endif ()
