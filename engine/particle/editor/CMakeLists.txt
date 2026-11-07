file(GLOB_RECURSE SRC_FILES src/* include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET Particle.Editor.Static STATIC
    SOURCES
        ${SRC_FILES}
    PUBLIC_INC
        include
    LINK_LIBS
        Particle
        Particle.Assets.Static
        Framework
        EditorCore
        EditorUi
        EditorWidgets
)
set_target_properties(Particle.Editor.Static PROPERTIES POSITION_INDEPENDENT_CODE ON)

sky_add_module(TARGET Particle.Editor
    SOURCES
        module/ParticleEditorModule.cpp
    LINK_LIBS
        Particle.Editor.Static
)
sky_add_dependency(TARGET Particle.Editor DEPENDENCIES SandboxEditor)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Particle.EditorTest
        SOURCES
            ${TEST_FILES}
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../../aurora/rhi/test
        LIBS
            Particle.Editor.Static
            3rdParty::googletest
    )
endif ()
