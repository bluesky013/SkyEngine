file(GLOB_RECURSE SOFTBODY_EDITOR_SOURCES src/*.cpp include/*.h)
sky_add_library(TARGET SoftBody.Editor.Static STATIC
    SOURCES ${SOFTBODY_EDITOR_SOURCES}
    PUBLIC_INC include
    LINK_LIBS SoftBody.Assets.Static SoftBody.Authoring.Static EditorCore Aurora.Render.Editor
)
sky_add_module(TARGET SoftBody.Editor
    SOURCES module/SoftBodyEditorModule.cpp
    LINK_LIBS SoftBody.Editor.Static
)
sky_add_dependency(TARGET SoftBody.Editor DEPENDENCIES SandboxEditor)
if (SKY_BUILD_TEST)
    file(GLOB SOFTBODY_EDITOR_TESTS test/*.cpp)
    sky_add_test(TARGET SoftBody.EditorTest SOURCES ${SOFTBODY_EDITOR_TESTS}
        LIBS SoftBody.Editor.Static 3rdParty::googletest)
endif ()
