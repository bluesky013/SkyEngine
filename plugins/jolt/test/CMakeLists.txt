file(GLOB TEST_SOURCES *.cpp)
sky_add_test(TARGET JoltPhysicsTest
    SOURCES ${TEST_SOURCES}
    INCS ../shared ../adaptor/src/assets ../adaptor/src/backend ../render
    LIBS JoltPhysics.Adaptor.Static JoltPhysics.Render.Static JoltPhysics.Builder.Static AuroraCook.Static 3rdParty::jolt 3rdParty::googletest
    WORKING_DIR $<TARGET_FILE_DIR:JoltPhysicsTest>
)
add_dependencies(JoltPhysicsTest JoltPhysicsModule JoltPhysicsRenderModule JoltPhysics.Builder)
if (NOT SKY_MODULE_PACKAGING STREQUAL "static")
    add_test(NAME JoltPhysicsModuleLifecycle COMMAND JoltPhysicsTest --module-lifecycle
        WORKING_DIRECTORY $<TARGET_FILE_DIR:JoltPhysicsTest>)
endif ()
