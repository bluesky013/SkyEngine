//
// Created on 2026/09/22.
//

#include <terrain/TerrainGenerator.h>
#include <terrain/TerrainAddress.h>

#include <core/math/PerlinNoise.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace sky::terrain {

    namespace {

        double Fbm(const PerlinNoise &perlin, const TerrainGenerateConfig &config, double x, double z)
        {
            const auto &noise = config.noise;

            double result = 0.0;
            double amp    = 1.0;
            double freq   = 1.0;
            double maxAmp = 0.0;

            const int octaves = noise.octaves > 0 ? noise.octaves : 1;
            for (int i = 0; i < octaves; ++i) {
                result += perlin.Octave2D_01(x * freq, z * freq, 1) * amp;
                maxAmp += amp;
                amp *= static_cast<double>(noise.gain);
                freq *= static_cast<double>(noise.lacunarity);
            }

            double value = maxAmp > 0.0 ? result / maxAmp : 0.0;
            if (noise.ridgedStrength > 0.f) {
                const double ridged = 1.0 - std::abs(2.0 * value - 1.0);
                value = value * (1.0 - noise.ridgedStrength) + ridged * noise.ridgedStrength;
            }
            return value;
        }

        float EncodeSample(const TerrainMeta &meta, double worldHeight)
        {
            if (meta.heightFormat == TerrainHeightFormat::R16_UNORM) {
                const double scale  = meta.heightScale != 0.f ? meta.heightScale : 1.0;
                const double norm   = (worldHeight - meta.heightOffset) / scale;
                const double clamped = std::min(std::max(norm, 0.0), 1.0);
                return static_cast<float>(std::round(clamped * 65535.0));
            }
            return static_cast<float>(worldHeight);
        }

        void WriteHeightSamples(const TerrainMeta &meta, const std::vector<float> &worldHeights, std::vector<uint8_t> &out)
        {
            const uint32_t count = static_cast<uint32_t>(worldHeights.size());
            if (meta.heightFormat == TerrainHeightFormat::R16_UNORM) {
                out.resize(static_cast<size_t>(count) * sizeof(uint16_t));
                for (uint32_t i = 0; i < count; ++i) {
                    const auto raw = static_cast<uint16_t>(EncodeSample(meta, worldHeights[i]));
                    std::memcpy(out.data() + static_cast<size_t>(i) * sizeof(uint16_t), &raw, sizeof(uint16_t));
                }
            } else {
                out.resize(static_cast<size_t>(count) * sizeof(float));
                std::memcpy(out.data(), worldHeights.data(), out.size());
            }
        }

        void DownsampleHeight(const std::vector<float> &in, uint32_t sizeIn, std::vector<float> &out, uint32_t sizeOut)
        {
            out.resize(static_cast<size_t>(sizeOut) * sizeOut);
            const uint32_t maxIndex = sizeIn - 1;

            for (uint32_t z = 0; z < sizeOut; ++z) {
                for (uint32_t x = 0; x < sizeOut; ++x) {
                    const uint32_t fx = std::min(2u * x, maxIndex);
                    const uint32_t fz = std::min(2u * z, maxIndex);

                    float sum = in[static_cast<size_t>(fz) * sizeIn + fx];
                    uint32_t n = 1;
                    if (fx + 1 < sizeIn) {
                        sum += in[static_cast<size_t>(fz) * sizeIn + fx + 1];
                        ++n;
                    }
                    if (fz + 1 < sizeIn) {
                        sum += in[static_cast<size_t>(fz + 1) * sizeIn + fx];
                        ++n;
                        if (fx + 1 < sizeIn) {
                            sum += in[static_cast<size_t>(fz + 1) * sizeIn + fx + 1];
                            ++n;
                        }
                    }
                    out[static_cast<size_t>(z) * sizeOut + x] = sum / static_cast<float>(n);
                }
            }
        }

        void DownsampleSplat(const std::vector<uint8_t> &in, uint32_t sizeIn, std::vector<uint8_t> &out, uint32_t sizeOut)
        {
            out.assign(static_cast<size_t>(sizeOut) * sizeOut * 4u, 0);
            const uint32_t maxIndex = sizeIn - 1;

            for (uint32_t z = 0; z < sizeOut; ++z) {
                for (uint32_t x = 0; x < sizeOut; ++x) {
                    const uint32_t fx = std::min(2u * x, maxIndex);
                    const uint32_t fz = std::min(2u * z, maxIndex);

                    for (uint32_t c = 0; c < 4u; ++c) {
                        uint32_t sum = in[(static_cast<size_t>(fz) * sizeIn + fx) * 4u + c];
                        uint32_t n   = 1;
                        if (fx + 1 < sizeIn) {
                            sum += in[(static_cast<size_t>(fz) * sizeIn + fx + 1) * 4u + c];
                            ++n;
                        }
                        if (fz + 1 < sizeIn) {
                            sum += in[(static_cast<size_t>(fz + 1) * sizeIn + fx) * 4u + c];
                            ++n;
                            if (fx + 1 < sizeIn) {
                                sum += in[(static_cast<size_t>(fz + 1) * sizeIn + fx + 1) * 4u + c];
                                ++n;
                            }
                        }
                        out[(static_cast<size_t>(z) * sizeOut + x) * 4u + c] = static_cast<uint8_t>(sum / n);
                    }
                }
            }
        }

        void GenerateSplat(const TerrainGenerateConfig &config, const TerrainMeta &meta,
                           const std::vector<float> &lod0, uint32_t size, std::vector<uint8_t> &out)
        {
            out.assign(static_cast<size_t>(size) * size * 4u, 0);

            const float res = meta.resolution > 0.f ? meta.resolution : 1.f;
            const uint32_t maxIndex = size - 1;

            for (uint32_t z = 0; z < size; ++z) {
                for (uint32_t x = 0; x < size; ++x) {
                    const float h = lod0[static_cast<size_t>(z) * size + x];

                    const float hl = lod0[static_cast<size_t>(z) * size + (x > 0 ? x - 1 : 0)];
                    const float hr = lod0[static_cast<size_t>(z) * size + (x < maxIndex ? x + 1 : maxIndex)];
                    const float hb = lod0[static_cast<size_t>(z > 0 ? z - 1 : 0) * size + x];
                    const float hf = lod0[static_cast<size_t>(z < maxIndex ? z + 1 : maxIndex) * size + x];

                    const float gx = (hr - hl) / (2.f * res);
                    const float gz = (hf - hb) / (2.f * res);
                    const float slopeDeg = std::atan(std::sqrt(gx * gx + gz * gz)) * 57.29577951308232f;

                    float weights[4] = {0.f, 0.f, 0.f, 0.f};
                    float sum = 0.f;
                    for (uint32_t i = 0; i < config.layerCount && i < 4u; ++i) {
                        const auto &rule = config.layers[i];
                        if (h >= rule.minHeight && h <= rule.maxHeight &&
                            slopeDeg >= rule.minSlope && slopeDeg <= rule.maxSlope) {
                            weights[i] = 1.f;
                            sum += 1.f;
                        }
                    }
                    if (sum <= 0.f) {
                        weights[0] = 1.f;
                        sum = 1.f;
                    }

                    for (uint32_t c = 0; c < 4u; ++c) {
                        out[(static_cast<size_t>(z) * size + x) * 4u + c] =
                            static_cast<uint8_t>(std::round(weights[c] / sum * 255.f));
                    }
                }
            }
        }

    } // namespace

    float SampleHeight01(const TerrainGenerateConfig &config, float worldX, float worldZ)
    {
        const PerlinNoise perlin(config.seed);
        const auto &noise = config.noise;

        double x = static_cast<double>(worldX) * noise.baseFrequency;
        double z = static_cast<double>(worldZ) * noise.baseFrequency;

        if (noise.warpStrength > 0.f) {
            const double wf = noise.warpFrequency;
            const double wx = (perlin.Octave2D_01(worldX * wf, worldZ * wf, 2) - 0.5) * 2.0;
            const double wz = (perlin.Octave2D_01(worldX * wf + 31.7, worldZ * wf + 11.3, 2) - 0.5) * 2.0;
            x += wx * noise.warpStrength;
            z += wz * noise.warpStrength;
        }

        return static_cast<float>(Fbm(perlin, config, x, z));
    }

    TerrainTilePayload GenerateTerrainTile(const TerrainGenerateConfig &config, const TerrainMeta &meta,
                                           const TerrainTileCoord &coord)
    {
        TerrainTilePayload payload;
        payload.coord = coord;

        const uint32_t lodCount = std::max(1u, config.lodCount);
        payload.lods.resize(lodCount);

        const uint32_t size0 = meta.GetTileVertexSize();
        const Vector3  origin = TileToWorld(meta, coord);
        const float    res = meta.resolution;

        std::vector<float> lod0(static_cast<size_t>(size0) * size0);
        for (uint32_t z = 0; z < size0; ++z) {
            for (uint32_t x = 0; x < size0; ++x) {
                const float worldX = origin.x + static_cast<float>(x) * res;
                const float worldZ = origin.z + static_cast<float>(z) * res;
                const float h01    = SampleHeight01(config, worldX, worldZ);
                lod0[static_cast<size_t>(z) * size0 + x] = config.heightOffset + h01 * config.heightScale;
            }
        }

        WriteHeightSamples(meta, lod0, payload.lods[0].height);
        GenerateSplat(config, meta, lod0, size0, payload.lods[0].splat.emplace_back());

        std::vector<float> current = std::move(lod0);
        uint32_t currentSize = size0;

        for (uint32_t lod = 1; lod < lodCount; ++lod) {
            const uint32_t sizeOut = meta.GetLodVertexSize(lod);

            std::vector<float> next;
            DownsampleHeight(current, currentSize, next, sizeOut);
            WriteHeightSamples(meta, next, payload.lods[lod].height);

            std::vector<uint8_t> splatOut;
            DownsampleSplat(payload.lods[lod - 1].splat[0], currentSize, splatOut, sizeOut);
            payload.lods[lod].splat.push_back(std::move(splatOut));

            current     = std::move(next);
            currentSize = sizeOut;
        }

        return payload;
    }

    bool TerrainTileGenerateTask::DoWork()
    {
        if (config == nullptr || meta == nullptr) {
            return false;
        }

        payload = GenerateTerrainTile(*config, *meta, coord);
        finished.store(true);
        return true;
    }

} // namespace sky::terrain
