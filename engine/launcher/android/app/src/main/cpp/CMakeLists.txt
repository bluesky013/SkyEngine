sky_add_library(TARGET Launcher SHARED
    SOURCES
        Main.cpp
        SampleScene.cpp
        SampleScene.h
    PRIVATE_INC
    PUBLIC_INC
    LINK_LIBS
        Framework
        RenderAdaptor
)
