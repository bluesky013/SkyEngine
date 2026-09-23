//
// Created on 2026/09/22.
//

#pragma once

#include <terrain/TerrainTypes.h>

#include <cstdint>

namespace sky {
    class SerializationContext;
} // namespace sky

namespace sky::terrain {

    struct TerrainNoiseConfig {
        float baseFrequency  = 0.005f;
        float lacunarity     = 2.f;
        float gain           = 0.5f;
        int   octaves        = 6;
        float warpStrength   = 0.f;
        float warpFrequency  = 0.01f;
        float ridgedStrength = 0.f;
    };

    // Height/slope band selecting a splat layer (plane 0, up to 4 layers).
    struct TerrainLayerRule {
        float minHeight = 0.f;
        float maxHeight = 0.f;
        float minSlope  = 0.f;
        float maxSlope  = 90.f;
    };

    struct TerrainGenerateConfig {
        uint32_t           seed = 0;
        TerrainNoiseConfig noise;
        float              heightScale  = 1.f;
        float              heightOffset = 0.f;
        uint32_t           layerCount   = 0;
        TerrainLayerRule   layers[4]    = {};
        uint32_t           lodCount     = 1;
    };

    // Flat, reflected authored source used by the offline builder (JSON).
    struct TerrainSourceData {
        uint32_t tileSize     = 64;
        float    resolution   = 1.f;
        uint32_t heightFormat = 0;   // cast to TerrainHeightFormat
        float    decodeScale  = 1.f;
        float    decodeOffset = 0.f;
        uint32_t tileCountX   = 0;
        uint32_t tileCountY   = 0;
        int32_t  tileStartX   = 0;
        int32_t  tileStartY   = 0;
        float    originX      = 0.f;
        float    originY      = 0.f;
        float    originZ      = 0.f;

        uint32_t seed           = 0;
        float    baseFrequency  = 0.005f;
        uint32_t octaves        = 6;
        float    lacunarity     = 2.f;
        float    gain           = 0.5f;
        float    warpStrength   = 0.f;
        float    warpFrequency  = 0.01f;
        float    ridgedStrength = 0.f;
        float    heightScale    = 1.f;
        float    heightOffset   = 0.f;
        uint32_t lodCount       = 1;
        uint32_t layerCount     = 0;

        static void Reflect(SerializationContext *context);
    };

    // Maps the flat authored source into runtime metadata + generation config.
    void ToGenerateConfig(const TerrainSourceData &source, TerrainMeta &outMeta, TerrainGenerateConfig &outConfig);

} // namespace sky::terrain
