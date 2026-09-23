//
// Created on 2026/09/22.
//

#pragma once

#include <vegetation/VegetationSurface.h>
#include <vegetation/VegetationTypes.h>

#include <vector>

namespace sky::vegetation {

    float SlopeDegrees(const Vector3 &normal);

    // Rule-gated density for one biome at a surface sample (0 when the sample does not match).
    float BiomeDensityAt(const VegetationBiome &biome, const VegetationSurfaceSample &sample);

    // Max rule-gated density over all biomes for a world position (presence/density query).
    float QueryDensity(const IVegetationSurfaceProvider &provider, const VegetationPalette &palette,
                       const Vector3 &worldPos, float densityScale = 1.f);

    // Deterministically populates one cell's instances in world space (order-independent).
    // Returns the number of instances emitted. densityScale applies distance density LOD.
    uint32_t GenerateCellInstances(const IVegetationSurfaceProvider &provider,
                                   const VegetationPlacementConfig &config,
                                   const VegetationPalette &palette,
                                   int32_t cellX, int32_t cellY,
                                   float densityScale,
                                   std::vector<VegetationInstance> &out);

} // namespace sky::vegetation
