sky_add_library(TARGET JoltPhysics.Builder.Static STATIC
    SOURCES src/PhysicsAssetBuilder.cpp
    PUBLIC_INC include
    PRIVATE_INC ../shared
    LINK_LIBS Physics Framework JoltPhysics.AssetCodec.Static
)
set_target_properties(JoltPhysics.Builder.Static PROPERTIES POSITION_INDEPENDENT_CODE ON)

sky_add_module(TARGET JoltPhysics.Builder
    SOURCES PhysicsBuilderModule.cpp
    LINK_LIBS JoltPhysics.Builder.Static
)
sky_add_dependency(TARGET JoltPhysics.Builder DEPENDENCIES Launcher SandboxEditor AssetTool)
