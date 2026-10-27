sky_add_library(TARGET JoltPhysics.AssetCodec.Static STATIC
    SOURCES PhysicsAssetCodec.cpp PhysicsAssetCodec.h
    LINK_LIBS Physics
)
set_target_properties(JoltPhysics.AssetCodec.Static PROPERTIES POSITION_INDEPENDENT_CODE ON)
