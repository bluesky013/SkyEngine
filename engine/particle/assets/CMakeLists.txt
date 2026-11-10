file(GLOB_RECURSE SRC_FILES src/* include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET Particle.Assets.Static STATIC
    SOURCES
        ${SRC_FILES}
    PUBLIC_INC
        include
    PUBLIC_LINK_LIBS
        Particle
        Framework
)

sky_add_module(TARGET Particle.Assets
    SOURCES
        module/ParticleAssetsModule.cpp
    LINK_LIBS
        Particle.Assets.Static
)
sky_add_dependency(TARGET Particle.Assets DEPENDENCIES Launcher SandboxEditor AssetTool)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Particle.AssetsTest
        SOURCES
            ${TEST_FILES}
            ${CMAKE_CURRENT_SOURCE_DIR}/../../aurora/rhi/test/main.cpp
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../../aurora/rhi/test
        LIBS
            Particle.Assets.Static
            3rdParty::googletest
    )
endif ()
