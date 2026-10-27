sky_add_module(TARGET JoltPhysicsRenderModule
    SOURCES JoltPhysicsRenderModule.cpp JoltMeshConversion.cpp JoltMeshConversion.h
    LINK_LIBS Physics Framework Aurora.Adaptor.Static
)
