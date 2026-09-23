//
// Created on 2026/09/22.
//

#include <vegetation/VegetationPlacement.h>

#include <algorithm>
#include <cmath>

namespace sky::vegetation {

    namespace {

        inline uint64_t SplitMix64(uint64_t x)
        {
            x += 0x9e3779b97f4a7c15ull;
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
            return x ^ (x >> 31);
        }

        inline uint64_t Hash(uint64_t a, uint64_t b)
        {
            return SplitMix64(a ^ (b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2)));
        }

        // Deterministic [0, 1) from a key + salt.
        inline float Rand01(uint64_t key, uint64_t salt)
        {
            return static_cast<float>((Hash(key, salt) >> 11) * (1.0 / 9007199254740992.0));
        }

        inline float RandRange(uint64_t key, uint64_t salt, float lo, float hi)
        {
            return lo + (hi - lo) * Rand01(key, salt);
        }

        inline uint32_t PickSpecies(const VegetationBiome &biome, uint64_t key)
        {
            float total = 0.f;
            for (const auto &species : biome.species) {
                total += std::max(species.density, 0.f);
            }
            if (total <= 0.f) {
                return 0;
            }

            float pick = Rand01(key, 0x54) * total;
            for (uint32_t i = 0; i < biome.species.size(); ++i) {
                pick -= std::max(biome.species[i].density, 0.f);
                if (pick <= 0.f) {
                    return i;
                }
            }
            return static_cast<uint32_t>(biome.species.size() - 1);
        }

    } // namespace

    float SlopeDegrees(const Vector3 &normal)
    {
        const float ny = std::min(std::max(normal.y, -1.f), 1.f);
        return std::acos(ny) * 57.29577951308232f;
    }

    float BiomeDensityAt(const VegetationBiome &biome, const VegetationSurfaceSample &sample)
    {
        if (!sample.valid) {
            return 0.f;
        }
        if (sample.height < biome.minHeight || sample.height > biome.maxHeight) {
            return 0.f;
        }

        const float slope = SlopeDegrees(sample.normal);
        if (slope < biome.minSlopeDeg || slope > biome.maxSlopeDeg) {
            return 0.f;
        }

        if (biome.layerIndex >= 0) {
            const int32_t index = biome.layerIndex;
            if (index >= 4 || sample.layerWeights[index] < biome.minLayerWeight) {
                return 0.f;
            }
        }

        return biome.density > 0.f ? biome.density : 0.f;
    }

    float QueryDensity(const IVegetationSurfaceProvider &provider, const VegetationPalette &palette,
                       const Vector3 &worldPos, float densityScale)
    {
        VegetationSurfaceSample sample;
        if (!provider.SampleSurface(worldPos, sample) || !sample.valid) {
            return 0.f;
        }

        float best = 0.f;
        for (const auto &biome : palette.biomes) {
            best = std::max(best, BiomeDensityAt(biome, sample) * densityScale);
        }
        return best;
    }

    uint32_t GenerateCellInstances(const IVegetationSurfaceProvider &provider,
                                   const VegetationPlacementConfig &config,
                                   const VegetationPalette &palette,
                                   int32_t cellX, int32_t cellY,
                                   float densityScale,
                                   std::vector<VegetationInstance> &out)
    {
        const float cellSize = provider.GetCellSize();
        if (cellSize <= 0.f || palette.biomes.empty() || densityScale <= 0.f) {
            return 0;
        }

        const AABB bounds = provider.GetCellBounds(cellX, cellY);
        const int width = static_cast<int>(std::round(std::sqrt(std::max(config.pointsPerSquareMeter, 0.f)) * cellSize));
        const int pointsPerAxis = std::max(1, width);

        const uint64_t cellKey = Hash(
            Hash(static_cast<uint64_t>(static_cast<uint32_t>(cellX)), static_cast<uint64_t>(static_cast<uint32_t>(cellY))),
            config.seed);

        uint32_t emitted = 0;
        for (int iz = 0; iz < pointsPerAxis; ++iz) {
            for (int ix = 0; ix < pointsPerAxis; ++ix) {
                const uint64_t key = Hash(cellKey, (static_cast<uint64_t>(iz) << 32) | static_cast<uint32_t>(ix));

                const float jx = Rand01(key, 0x51);
                const float jz = Rand01(key, 0x52);
                const float worldX = bounds.min.x + (static_cast<float>(ix) + jx) / static_cast<float>(pointsPerAxis) * cellSize;
                const float worldZ = bounds.min.z + (static_cast<float>(iz) + jz) / static_cast<float>(pointsPerAxis) * cellSize;

                Vector3 pos(worldX, 0.f, worldZ);
                VegetationSurfaceSample sample;
                if (!provider.SampleSurface(pos, sample) || !sample.valid) {
                    continue;
                }
                pos.y = sample.height;

                const VegetationBiome *chosen = nullptr;
                float bestDensity = 0.f;
                for (const auto &biome : palette.biomes) {
                    const float d = BiomeDensityAt(biome, sample);
                    if (d > bestDensity) {
                        bestDensity = d;
                        chosen = &biome;
                    }
                }
                if (chosen == nullptr || chosen->species.empty()) {
                    continue;
                }

                const float probability = std::min(bestDensity * densityScale, 1.f);
                if (Rand01(key, 0x53) >= probability) {
                    continue;
                }

                const uint32_t speciesIndex = PickSpecies(*chosen, key);
                const auto &species = chosen->species[speciesIndex];

                VegetationInstance instance;
                instance.position     = pos;
                instance.rotation     = RandRange(key, 0x55, 0.f, species.rotationJitter);
                instance.scale        = RandRange(key, 0x56, species.scaleMin, species.scaleMax);
                instance.biomeId      = chosen->id;
                instance.speciesIndex = speciesIndex;

                out.push_back(instance);
                ++emitted;
            }
        }

        return emitted;
    }

} // namespace sky::vegetation
