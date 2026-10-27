file(GLOB TEST_SOURCES *.cpp)
file(GLOB_RECURSE ADAPTOR_SOURCES ../adaptor/src/*.cpp)
sky_add_test(TARGET JoltPhysicsTest
    SOURCES ${TEST_SOURCES} ${ADAPTOR_SOURCES} ../render/JoltMeshConversion.cpp
    INCS ../adaptor/include ../adaptor/src/backend ../render
    LIBS Physics Framework Aurora.Adaptor.Static AuroraCook.Static 3rdParty::jolt 3rdParty::googletest
    WORKING_DIR $<TARGET_FILE_DIR:JoltPhysicsTest>
)
