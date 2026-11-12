file(GLOB_RECURSE SOFTBODY_BUILDER_SOURCES src/*.cpp include/*.h)
sky_add_library(TARGET SoftBody.Builder.Static STATIC
    SOURCES ${SOFTBODY_BUILDER_SOURCES}
    PUBLIC_INC include
    LINK_LIBS SoftBody.Assets.Static SoftBody.Authoring.Static
)
sky_add_module(TARGET SoftBody.Builder
    SOURCES module/SoftBodyBuilderModule.cpp
    LINK_LIBS SoftBody.Builder.Static
)
sky_add_dependency(TARGET SoftBody.Builder DEPENDENCIES Launcher SandboxEditor AssetTool)
if (SKY_BUILD_TEST)
    file(GLOB SOFTBODY_BUILDER_TESTS test/*.cpp)
    sky_add_test(TARGET SoftBody.BuilderTest SOURCES ${SOFTBODY_BUILDER_TESTS}
        LIBS SoftBody.Builder.Static AuroraCook.Static 3rdParty::googletest)
endif ()
