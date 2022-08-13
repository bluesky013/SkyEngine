file(GLOB_RECURSE ASSET_BUILDERS_SRC builders/*)
#
#sky_add_library(TARGET AssetToolModule SHARED
#    SOURCES
#        ${ASSET_TOOL_SRC}
#        ${ASSET_TOOL_INC}
#    PRIVATE_INC
#        src
#    PUBLIC_INC
#        include
#    LINK_LIBS
#        Framework
#    )


sky_add_exe(TARGET AssetTool ${PLATFORM_TYPE}
    SOURCES
        main.cpp
        ${ASSET_BUILDERS_SRC}
    INCS
        builders/include
    LIBS
        SkyRender
        3rdParty::cxxopts
        3rdParty::assimp
    )