sky_add_exe(TARGET AssetBrowser
    SOURCES
        main.cpp
    INCS
    LIBS
        Framework
        3rdParty::cxxopts
)

sky_set_dependency(TARGET AssetBrowser)