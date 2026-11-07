file(GLOB_RECURSE SRC_FILES src/* include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET Particle.Builder.Static STATIC
    SOURCES
        ${SRC_FILES}
    PUBLIC_INC
        include
    LINK_LIBS
        Particle
        Particle.Assets.Static
        Framework
)

sky_add_module(TARGET Particle.Builder
    SOURCES
        module/ParticleBuilderModule.cpp
    LINK_LIBS
        Particle.Builder.Static
)
sky_add_dependency(TARGET Particle.Builder DEPENDENCIES Launcher SandboxEditor AssetTool)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Particle.BuilderTest
        SOURCES
            ${TEST_FILES}
        LIBS
            Particle.Builder.Static
            3rdParty::googletest
    )
endif ()
