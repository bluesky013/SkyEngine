file(GLOB_RECURSE SRC_FILES src/* include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET Particle.Adaptor.Static STATIC
    SOURCES
        ${SRC_FILES}
    PUBLIC_INC
        include
    LINK_LIBS
        Particle
        Particle.Assets.Static
        Framework
)

sky_add_module(TARGET Particle.World
    SOURCES
        module/ParticleWorldModule.cpp
    LINK_LIBS
        Particle.Adaptor.Static
)
sky_add_dependency(TARGET Particle.World DEPENDENCIES Launcher SandboxEditor)

add_subdirectory(render)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Particle.WorldTest
        SOURCES
            ${TEST_FILES}
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../../aurora/rhi/test
        LIBS
            Particle.Adaptor.Static
            Particle.Builder.Static
            3rdParty::googletest
    )
    sky_add_test(TARGET Particle.ModuleTest
        SOURCES
            module_test/ParticleModuleTest.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../../aurora/rhi/test/main.cpp
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../../aurora/rhi/test
        LIBS
            Particle.Adaptor.Static
            Particle.Assets.Static
            Framework
            3rdParty::googletest
    )
    add_dependencies(Particle.ModuleTest Particle.Assets Particle.World Content.Module Content.Metadata)
endif ()
