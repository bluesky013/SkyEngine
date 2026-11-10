file(GLOB_RECURSE SRC_FILES src/* include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET Particle.Builder.Static STATIC
    SOURCES
        ${SRC_FILES}
    PUBLIC_INC
        include
    PRIVATE_LINK_LIBS
        Particle
        Particle.Assets.Static
        Framework
)

if (SKY_DEVELOP)
    sky_add_module(TARGET Particle.Builder
        SOURCES
            module/ParticleBuilderModule.cpp
        PRIVATE_LINK_LIBS
            Particle.Builder.Static
            Framework
    )
    sky_add_dependency(TARGET Particle.Builder DEPENDENCIES SandboxEditor AssetTool)
endif ()

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Particle.BuilderTest
        SOURCES
            ${TEST_FILES}
        LIBS
            Particle.Builder.Static
            Particle.Assets.Static
            3rdParty::googletest
    )
endif ()
