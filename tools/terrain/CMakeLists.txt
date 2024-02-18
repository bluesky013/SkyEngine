file(GLOB_RECURSE TERRAIN_GEN_SRC ./*)

sky_add_exe(TARGET TerrainGenerator ${PLATFORM_BUNDLE}
        SOURCES
            ${TERRAIN_GEN_SRC}
        INCS
            include
        LIBS
            3rdParty::cxxopts
            3rdParty::stb
            3rdParty::ktx
            3rdParty::PerlinNoise
            VulkanRHI
        )
