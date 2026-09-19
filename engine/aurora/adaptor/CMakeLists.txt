file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

# Static aurora -> framework bridge (analog of legacy RenderAdaptor):
# reflection + assets + components + the IModule implementation (AuroraModule).
# The launcher/editor module entry points live in engine/aurora/{runtime,editor}.
sky_add_library(TARGET Aurora.Adaptor STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PUBLIC_INC
        include
    LINK_LIBS
        Aurora
        Aurora.RHI
        Framework
        Core
)

file(GLOB_RECURSE TEST_FILES test/*)
if (SKY_BUILD_TEST)
    sky_add_test(TARGET AuroraAdaptorTest
        SOURCES
            ${TEST_FILES}
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test/main.cpp
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test
        LIBS
            Aurora.Adaptor
            Aurora.RHI
            Core
            3rdParty::googletest
    )
endif ()
