sky_add_exe(TARGET AssetBuilder
    SOURCES
        main.cpp
    INCS
    LIBS
        Framework
        3rdParty::cxxopts
)

sky_set_dependency(TARGET AssetBuilder)