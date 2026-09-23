//
// Created on 2026/09/22.
//

#pragma once

#include <core/math/Vector3.h>
#include <core/util/Uuid.h>

#include <cstdint>
#include <vector>

namespace sky::vegetation {

    struct VegetationSpecies {
        Uuid  mesh;                  // mesh or billboard asset
        float density        = 1.f;  // relative weight within the biome
        float scaleMin       = 0.8f;
        float scaleMax       = 1.2f;
        float rotationJitter = 360.f; // yaw range in degrees
        float windResponse   = 1.f;
    };

    // A vegetation zone: placement rules + species palette + density. Driven by surface properties.
    struct VegetationBiome {
        uint32_t id = 0;

        float    minSlopeDeg = 0.f;
        float    maxSlopeDeg = 45.f;
        float    minHeight   = -1.0e30f;
        float    maxHeight   = 1.0e30f;

        int32_t  layerIndex    = -1;   // < 0 = no surface-layer gate
        float    minLayerWeight = 0.5f;

        float    density = 1.f;        // base instance density multiplier
        std::vector<VegetationSpecies> species;
    };

    struct VegetationPalette {
        std::vector<VegetationBiome> biomes;
    };

    struct VegetationPlacementConfig {
        uint32_t seed = 0;
        float    pointsPerSquareMeter = 1.f; // candidate sample density
    };

    struct VegetationInstance {
        Vector3  position;
        float    rotation = 0.f;   // yaw degrees
        float    scale    = 1.f;
        uint32_t biomeId  = 0;
        uint32_t speciesIndex = 0;
    };

} // namespace sky::vegetation
