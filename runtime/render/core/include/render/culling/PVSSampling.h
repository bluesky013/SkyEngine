//
// Created by SkyEngine on 2024/02/26.
//

#pragma once

#include <vector>
#include <cstdint>
#include <cmath>
#include <functional>
#include <render/culling/PVSTypes.h>
#include <core/math/Vector3.h>
#include <core/shapes/AABB.h>

namespace sky {

    /**
     * @brief Sampling strategy for generating sample points/directions
     */
    enum class PVSSamplingStrategy {
        RANDOM,         // Pure random sampling
        STRATIFIED,     // Stratified (jittered grid) sampling
        HALTON,         // Halton low-discrepancy sequence
        FIBONACCI       // Fibonacci sphere sampling for directions
    };

    /**
     * @brief Configuration for cell sampling
     */
    struct PVSCellSamplingConfig {
        uint32_t numSamplesPerCell = 64;        // Number of sample points per cell
        uint32_t numDirectionsPerSample = 32;   // Number of directions per sample point
        PVSSamplingStrategy pointStrategy = PVSSamplingStrategy::STRATIFIED;
        PVSSamplingStrategy directionStrategy = PVSSamplingStrategy::FIBONACCI;
        bool useHemisphereForFloors = false;    // Use hemisphere sampling for floor surfaces
        float hemisphereUpBias = 0.0f;          // Bias towards up direction (0-1)
    };

    /**
     * @brief A single sample with position and direction
     */
    struct PVSSample {
        Vector3 position;
        Vector3 direction;
    };

    /**
     * @brief Collection of samples for a cell
     */
    struct PVSCellSamples {
        PVSCellID cellId = INVALID_PVS_CELL;
        std::vector<PVSSample> samples;
    };

    /**
     * @brief Simple random number generator for sampling
     * 
     * Uses xorshift128+ algorithm for fast, quality random numbers
     */
    class PVSRandomGenerator {
    public:
        explicit PVSRandomGenerator(uint64_t seed = 12345ULL)
        {
            state[0] = seed;
            state[1] = seed ^ 0x5DEECE66DULL;
            // Warm up
            for (int i = 0; i < 10; ++i) {
                NextU64();
            }
        }

        /**
         * @brief Generate random uint64
         */
        uint64_t NextU64()
        {
            uint64_t s1 = state[0];
            uint64_t s0 = state[1];
            uint64_t result = s0 + s1;
            state[0] = s0;
            s1 ^= s1 << 23;
            state[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
            return result;
        }

        /**
         * @brief Generate random float in [0, 1)
         */
        float NextFloat()
        {
            return static_cast<float>(NextU64() & 0xFFFFFFFFULL) / 4294967296.0f;
        }

        /**
         * @brief Generate random float in [min, max)
         */
        float NextFloat(float min, float max)
        {
            return min + NextFloat() * (max - min);
        }

        /**
         * @brief Seed the generator
         */
        void Seed(uint64_t seed)
        {
            state[0] = seed;
            state[1] = seed ^ 0x5DEECE66DULL;
        }

    private:
        uint64_t state[2];
    };

    /**
     * @brief Sampling utility class for PVS visibility computation
     */
    class PVSSampling {
    public:
        PVSSampling() : rng(12345ULL) {}
        explicit PVSSampling(uint64_t seed) : rng(seed) {}

        /**
         * @brief Generate a random point within a cell/AABB
         * @param bounds The cell bounds
         * @return Random point within the bounds
         */
        Vector3 GenerateRandomPointInCell(const AABB &bounds)
        {
            return Vector3(
                rng.NextFloat(bounds.min.x, bounds.max.x),
                rng.NextFloat(bounds.min.y, bounds.max.y),
                rng.NextFloat(bounds.min.z, bounds.max.z)
            );
        }

        /**
         * @brief Generate a random direction on unit sphere
         * 
         * Uses spherical coordinates with uniform distribution
         * @return Normalized direction vector
         */
        Vector3 GenerateRandomDirection()
        {
            // Generate uniform random point on unit sphere
            // Using spherical coordinates with theta = [0, 2pi], phi = [0, pi]
            float u = rng.NextFloat();
            float v = rng.NextFloat();
            
            float theta = 2.0f * PI * u;
            float phi = std::acos(2.0f * v - 1.0f);  // Uniform distribution on sphere
            
            float sinPhi = std::sin(phi);
            return Vector3(
                sinPhi * std::cos(theta),
                sinPhi * std::sin(theta),
                std::cos(phi)
            );
        }

        /**
         * @brief Generate a random direction in hemisphere around normal
         * @param normal The hemisphere normal direction
         * @return Normalized direction in the hemisphere
         */
        Vector3 GenerateHemisphereDirection(const Vector3 &normal)
        {
            // Generate random point on hemisphere (cosine-weighted)
            float u = rng.NextFloat();
            float v = rng.NextFloat();
            
            float phi = 2.0f * PI * u;
            float cosTheta = std::sqrt(1.0f - v);  // Cosine-weighted
            float sinTheta = std::sqrt(v);
            
            // Local hemisphere direction
            Vector3 localDir(
                sinTheta * std::cos(phi),
                sinTheta * std::sin(phi),
                cosTheta
            );
            
            // Build orthonormal basis around normal
            Vector3 up = std::abs(normal.z) < 0.999f ? Vector3(0, 0, 1) : Vector3(1, 0, 0);
            Vector3 tangent = up.Cross(normal);
            tangent.Normalize();
            Vector3 bitangent = normal.Cross(tangent);
            
            // Transform to world space
            return Vector3(
                tangent.x * localDir.x + bitangent.x * localDir.y + normal.x * localDir.z,
                tangent.y * localDir.x + bitangent.y * localDir.y + normal.y * localDir.z,
                tangent.z * localDir.x + bitangent.z * localDir.y + normal.z * localDir.z
            );
        }

        /**
         * @brief Generate Halton sequence value
         * @param index Sample index (1-based)
         * @param base Base for the sequence (usually prime: 2, 3, 5, etc.)
         * @return Value in [0, 1)
         */
        static float HaltonSequence(uint32_t index, uint32_t base)
        {
            float result = 0.0f;
            float f = 1.0f / static_cast<float>(base);
            uint32_t i = index;
            
            while (i > 0) {
                result += f * static_cast<float>(i % base);
                i /= base;
                f /= static_cast<float>(base);
            }
            
            return result;
        }

        /**
         * @brief Generate Fibonacci sphere direction
         * 
         * Generates uniformly distributed points on a sphere using Fibonacci spiral
         * @param index Sample index
         * @param numSamples Total number of samples
         * @return Direction on unit sphere
         */
        static Vector3 FibonacciSphereDirection(uint32_t index, uint32_t numSamples)
        {
            static constexpr float GOLDEN_RATIO = 1.6180339887498948482f;
            
            float y = 1.0f - (2.0f * static_cast<float>(index) + 1.0f) / static_cast<float>(numSamples);
            float radius = std::sqrt(1.0f - y * y);
            float theta = 2.0f * PI * static_cast<float>(index) / GOLDEN_RATIO;
            
            return Vector3(
                radius * std::cos(theta),
                y,
                radius * std::sin(theta)
            );
        }

        /**
         * @brief Generate stratified sample points in a cell
         * 
         * Divides the cell into strata and samples one point per stratum
         * @param bounds Cell bounds
         * @param numSamples Number of samples to generate
         * @param outPoints Output vector of sample points
         */
        void GenerateStratifiedPoints(
            const AABB &bounds,
            uint32_t numSamples,
            std::vector<Vector3> &outPoints)
        {
            outPoints.clear();
            outPoints.reserve(numSamples);
            
            // Compute strata grid dimensions
            uint32_t n = static_cast<uint32_t>(std::ceil(std::cbrt(static_cast<float>(numSamples))));
            
            Vector3 size = bounds.max - bounds.min;
            Vector3 strataSize(
                size.x / static_cast<float>(n),
                size.y / static_cast<float>(n),
                size.z / static_cast<float>(n)
            );
            
            uint32_t count = 0;
            for (uint32_t z = 0; z < n && count < numSamples; ++z) {
                for (uint32_t y = 0; y < n && count < numSamples; ++y) {
                    for (uint32_t x = 0; x < n && count < numSamples; ++x) {
                        // Compute stratum bounds
                        Vector3 strataMin(
                            bounds.min.x + static_cast<float>(x) * strataSize.x,
                            bounds.min.y + static_cast<float>(y) * strataSize.y,
                            bounds.min.z + static_cast<float>(z) * strataSize.z
                        );
                        Vector3 strataMax = strataMin + strataSize;
                        
                        // Random point within stratum
                        outPoints.emplace_back(
                            rng.NextFloat(strataMin.x, strataMax.x),
                            rng.NextFloat(strataMin.y, strataMax.y),
                            rng.NextFloat(strataMin.z, strataMax.z)
                        );
                        ++count;
                    }
                }
            }
        }

        /**
         * @brief Generate Halton sequence points in a cell
         * @param bounds Cell bounds
         * @param numSamples Number of samples to generate
         * @param outPoints Output vector of sample points
         */
        void GenerateHaltonPoints(
            const AABB &bounds,
            uint32_t numSamples,
            std::vector<Vector3> &outPoints)
        {
            outPoints.clear();
            outPoints.reserve(numSamples);
            
            Vector3 size = bounds.max - bounds.min;
            
            for (uint32_t i = 1; i <= numSamples; ++i) {
                Vector3 point(
                    bounds.min.x + HaltonSequence(i, 2) * size.x,
                    bounds.min.y + HaltonSequence(i, 3) * size.y,
                    bounds.min.z + HaltonSequence(i, 5) * size.z
                );
                outPoints.emplace_back(point);
            }
        }

        /**
         * @brief Generate random directions on unit sphere
         * @param numDirections Number of directions to generate
         * @param outDirections Output vector of directions
         */
        void GenerateRandomDirections(
            uint32_t numDirections,
            std::vector<Vector3> &outDirections)
        {
            outDirections.clear();
            outDirections.reserve(numDirections);
            
            for (uint32_t i = 0; i < numDirections; ++i) {
                outDirections.emplace_back(GenerateRandomDirection());
            }
        }

        /**
         * @brief Generate Fibonacci sphere directions
         * @param numDirections Number of directions to generate
         * @param outDirections Output vector of directions
         */
        static void GenerateFibonacciDirections(
            uint32_t numDirections,
            std::vector<Vector3> &outDirections)
        {
            outDirections.clear();
            outDirections.reserve(numDirections);
            
            for (uint32_t i = 0; i < numDirections; ++i) {
                outDirections.emplace_back(FibonacciSphereDirection(i, numDirections));
            }
        }

        /**
         * @brief Generate complete cell samples (points + directions)
         * 
         * Main function for generating sample points within a cell and
         * sampling directions for each point, used in ray-based PVS baking.
         * 
         * @param cellBounds Bounds of the PVS cell
         * @param config Sampling configuration
         * @param outSamples Output cell samples
         */
        void GenerateCellSamples(
            const AABB &cellBounds,
            const PVSCellSamplingConfig &config,
            PVSCellSamples &outSamples)
        {
            outSamples.samples.clear();
            outSamples.samples.reserve(config.numSamplesPerCell * config.numDirectionsPerSample);
            
            // Generate sample points
            std::vector<Vector3> points;
            switch (config.pointStrategy) {
                case PVSSamplingStrategy::RANDOM:
                    points.reserve(config.numSamplesPerCell);
                    for (uint32_t i = 0; i < config.numSamplesPerCell; ++i) {
                        points.emplace_back(GenerateRandomPointInCell(cellBounds));
                    }
                    break;
                case PVSSamplingStrategy::STRATIFIED:
                    GenerateStratifiedPoints(cellBounds, config.numSamplesPerCell, points);
                    break;
                case PVSSamplingStrategy::HALTON:
                    GenerateHaltonPoints(cellBounds, config.numSamplesPerCell, points);
                    break;
                default:
                    // Default to stratified
                    GenerateStratifiedPoints(cellBounds, config.numSamplesPerCell, points);
                    break;
            }
            
            // Generate directions
            std::vector<Vector3> directions;
            switch (config.directionStrategy) {
                case PVSSamplingStrategy::RANDOM:
                    GenerateRandomDirections(config.numDirectionsPerSample, directions);
                    break;
                case PVSSamplingStrategy::FIBONACCI:
                    GenerateFibonacciDirections(config.numDirectionsPerSample, directions);
                    break;
                default:
                    GenerateFibonacciDirections(config.numDirectionsPerSample, directions);
                    break;
            }
            
            // Combine points and directions
            for (const auto &point : points) {
                if (config.useHemisphereForFloors) {
                    // For floor cells, only cast rays upward
                    Vector3 upNormal(0, 1, 0);
                    for (uint32_t i = 0; i < config.numDirectionsPerSample; ++i) {
                        PVSSample sample;
                        sample.position = point;
                        sample.direction = GenerateHemisphereDirection(upNormal);
                        outSamples.samples.push_back(sample);
                    }
                } else {
                    // Full sphere sampling
                    for (const auto &dir : directions) {
                        PVSSample sample;
                        sample.position = point;
                        sample.direction = dir;
                        outSamples.samples.push_back(sample);
                    }
                }
            }
        }

        /**
         * @brief Generate sample points within cell (convenience function)
         * @param cellBounds Cell bounds
         * @param numSamples Number of samples
         * @param strategy Sampling strategy
         * @param outPoints Output points
         */
        void GeneratePointsInCell(
            const AABB &cellBounds,
            uint32_t numSamples,
            PVSSamplingStrategy strategy,
            std::vector<Vector3> &outPoints)
        {
            switch (strategy) {
                case PVSSamplingStrategy::RANDOM:
                    outPoints.clear();
                    outPoints.reserve(numSamples);
                    for (uint32_t i = 0; i < numSamples; ++i) {
                        outPoints.emplace_back(GenerateRandomPointInCell(cellBounds));
                    }
                    break;
                case PVSSamplingStrategy::STRATIFIED:
                    GenerateStratifiedPoints(cellBounds, numSamples, outPoints);
                    break;
                case PVSSamplingStrategy::HALTON:
                    GenerateHaltonPoints(cellBounds, numSamples, outPoints);
                    break;
                default:
                    GenerateStratifiedPoints(cellBounds, numSamples, outPoints);
                    break;
            }
        }

        /**
         * @brief Generate sample directions (convenience function)
         * @param numDirections Number of directions
         * @param strategy Sampling strategy
         * @param outDirections Output directions
         */
        void GenerateDirections(
            uint32_t numDirections,
            PVSSamplingStrategy strategy,
            std::vector<Vector3> &outDirections)
        {
            switch (strategy) {
                case PVSSamplingStrategy::RANDOM:
                    GenerateRandomDirections(numDirections, outDirections);
                    break;
                case PVSSamplingStrategy::FIBONACCI:
                    GenerateFibonacciDirections(numDirections, outDirections);
                    break;
                default:
                    GenerateFibonacciDirections(numDirections, outDirections);
                    break;
            }
        }

        /**
         * @brief Set the random seed
         */
        void SetSeed(uint64_t seed)
        {
            rng.Seed(seed);
        }

    private:
        static constexpr float PI = 3.14159265358979323846f;
        PVSRandomGenerator rng;
    };

} // namespace sky
