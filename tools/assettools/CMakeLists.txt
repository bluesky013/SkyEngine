sky_add_exe(TARGET AssetTool ${PLATFORM_TYPE}
    SOURCES
        main.cpp
    INCS
        builders/include
    LIBS
        Framework
        3rdParty::cxxopts
        3rdParty::assimp
    )
