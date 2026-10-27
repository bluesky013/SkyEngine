sky_add_library(TARGET JoltPhysics.Render.Static STATIC
    SOURCES JoltMeshConversion.cpp JoltMeshConversion.h
    LINK_LIBS Physics Framework Aurora.Adaptor.Static
)
set_target_properties(JoltPhysics.Render.Static PROPERTIES POSITION_INDEPENDENT_CODE ON)

sky_add_module(TARGET JoltPhysicsRenderModule
    SOURCES JoltPhysicsRenderModule.cpp
    LINK_LIBS JoltPhysics.Render.Static
)
