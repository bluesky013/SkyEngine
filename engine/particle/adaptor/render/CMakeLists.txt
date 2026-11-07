file(GLOB_RECURSE SRC_FILES src/* include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET Particle.Render.Static STATIC
    SOURCES
        ${SRC_FILES}
    PRIVATE_INC
        decl
    PUBLIC_INC
        include
    LINK_LIBS
        Particle
        Aurora.Core.Static
        Aurora.Adaptor.Static
        Framework
)

sky_add_module(TARGET Particle.Render
    SOURCES
        module/ParticleRenderModule.cpp
    LINK_LIBS
        Particle.Render.Static
)
sky_add_dependency(TARGET Particle.Render DEPENDENCIES Launcher SandboxEditor)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Particle.RenderTest
        SOURCES
            ${TEST_FILES}
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../aurora/rhi/test/main.cpp
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/decl
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../aurora/rhi/test
        LIBS
            Particle.Render.Static
            3rdParty::googletest
    )
endif ()
