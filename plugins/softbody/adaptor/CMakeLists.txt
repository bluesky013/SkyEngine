file(GLOB_RECURSE SOFTBODY_WORLD_SOURCES src/*.cpp include/*.h)
sky_add_library(TARGET SoftBody.World.Static STATIC
    SOURCES ${SOFTBODY_WORLD_SOURCES}
    PUBLIC_INC include
    LINK_LIBS SoftBody.Assets.Static SoftBody.Solver.Static Physics.SoftBody.Interface Animation Framework
)
sky_add_module(TARGET SoftBody.World
    SOURCES module/SoftBodyWorldModule.cpp
    LINK_LIBS SoftBody.World.Static
)
sky_add_dependency(TARGET SoftBody.World DEPENDENCIES Launcher SandboxEditor)
if (SKY_BUILD_TEST AND TARGET JoltPhysics.Adaptor.Static)
    file(GLOB SOFTBODY_WORLD_TESTS test/*.cpp)
    sky_add_test(TARGET SoftBody.WorldTest SOURCES ${SOFTBODY_WORLD_TESTS}
        INCS ${ENGINE_ROOT}/plugins/jolt/adaptor/src/backend
        LIBS SoftBody.World.Static JoltPhysics.Adaptor.Static 3rdParty::googletest)
endif ()
