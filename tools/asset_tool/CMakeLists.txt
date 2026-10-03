sky_add_exe(TARGET AssetTool
    SOURCES
        main.cpp
        CookWorkerHost.cpp
    LIBS
        Framework
)

sky_set_dependency(TARGET AssetTool)
