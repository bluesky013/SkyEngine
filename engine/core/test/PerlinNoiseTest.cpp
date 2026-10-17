//
// Created by blues on 2026/3/16.
//

#include <gtest/gtest.h>
#include <core/math/PerlinNoise.h>
#include <chrono>
#include <cmath>

using namespace sky;

// ---------------------------------------------------------------------------
// Correctness: output range [0, 1]
// ---------------------------------------------------------------------------
TEST(PerlinNoiseTest, Octave2DOutputRange)
{
    PerlinNoise perlin{42};
    for (int i = 0; i < 1000; ++i) {
        double x = (i % 100) * 0.1;
        double y = (i / 100) * 0.1;
        double v = perlin.Octave2D_01(x, y, 4);
        ASSERT_GE(v, 0.0) << "x=" << x << " y=" << y;
        ASSERT_LE(v, 1.0) << "x=" << x << " y=" << y;
    }
}

TEST(PerlinNoiseTest, Octave3DOutputRange)
{
    PerlinNoise perlin{42};
    for (int i = 0; i < 1000; ++i) {
        double x = (i % 10) * 0.1;
        double y = ((i / 10) % 10) * 0.1;
        double z = (i / 100) * 0.1;
        double v = perlin.Octave3D_01(x, y, z, 4);
        ASSERT_GE(v, 0.0) << "x=" << x << " y=" << y << " z=" << z;
        ASSERT_LE(v, 1.0) << "x=" << x << " y=" << y << " z=" << z;
    }
}

// ---------------------------------------------------------------------------
// Determinism: same seed → same output
// ---------------------------------------------------------------------------
TEST(PerlinNoiseTest, Deterministic2D)
{
    PerlinNoise a{12345};
    PerlinNoise b{12345};
    for (int i = 0; i < 100; ++i) {
        double x = i * 0.37;
        double y = i * 0.53;
        ASSERT_EQ(a.Octave2D_01(x, y, 3), b.Octave2D_01(x, y, 3));
    }
}

TEST(PerlinNoiseTest, Deterministic3D)
{
    PerlinNoise a{12345};
    PerlinNoise b{12345};
    for (int i = 0; i < 100; ++i) {
        double x = i * 0.37;
        double y = i * 0.53;
        double z = i * 0.71;
        ASSERT_EQ(a.Octave3D_01(x, y, z, 3), b.Octave3D_01(x, y, z, 3));
    }
}

// ---------------------------------------------------------------------------
// Different seeds → different permutations → different output
// ---------------------------------------------------------------------------
TEST(PerlinNoiseTest, DifferentSeeds)
{
    PerlinNoise a{1};
    PerlinNoise b{2};

    int diffCount = 0;
    for (int i = 0; i < 100; ++i) {
        double x = i * 0.5;
        double y = i * 0.7;
        if (a.Octave2D_01(x, y, 3) != b.Octave2D_01(x, y, 3)) {
            ++diffCount;
        }
    }
    ASSERT_GT(diffCount, 50);
}

// ---------------------------------------------------------------------------
// Continuity: nearby inputs → small difference
// ---------------------------------------------------------------------------
TEST(PerlinNoiseTest, Continuity2D)
{
    PerlinNoise perlin{0};
    double epsilon = 1e-6;
    for (int i = 0; i < 100; ++i) {
        double x = i * 0.3;
        double y = i * 0.7;
        double v0 = perlin.Octave2D_01(x, y, 3);
        double v1 = perlin.Octave2D_01(x + epsilon, y, 3);
        ASSERT_NEAR(v0, v1, 1e-3) << "Continuity broken at x=" << x << " y=" << y;
    }
}

TEST(PerlinNoiseTest, Continuity3D)
{
    PerlinNoise perlin{0};
    double epsilon = 1e-6;
    for (int i = 0; i < 100; ++i) {
        double x = i * 0.3;
        double y = i * 0.5;
        double z = i * 0.7;
        double v0 = perlin.Octave3D_01(x, y, z, 3);
        double v1 = perlin.Octave3D_01(x + epsilon, y, z, 3);
        ASSERT_NEAR(v0, v1, 1e-3) << "Continuity broken at x=" << x << " y=" << y << " z=" << z;
    }
}

// ---------------------------------------------------------------------------
// Integer lattice points: noise should be exactly 0.5
//   At integer coords, fractional part = 0, Fade(0) = 0,
//   all gradients evaluate to 0 → raw noise = 0 → octave_01 = 0.5
// ---------------------------------------------------------------------------
TEST(PerlinNoiseTest, IntegerPointsReturnHalf2D)
{
    PerlinNoise perlin{7};
    for (int x = 0; x < 10; ++x) {
        for (int y = 0; y < 10; ++y) {
            double v = perlin.Octave2D_01(x, y, 1);
            ASSERT_DOUBLE_EQ(v, 0.5) << "x=" << x << " y=" << y;
        }
    }
}

TEST(PerlinNoiseTest, IntegerPointsReturnHalf3D)
{
    PerlinNoise perlin{7};
    for (int x = 0; x < 5; ++x) {
        for (int y = 0; y < 5; ++y) {
            for (int z = 0; z < 5; ++z) {
                double v = perlin.Octave3D_01(x, y, z, 1);
                ASSERT_DOUBLE_EQ(v, 0.5) << "x=" << x << " y=" << y << " z=" << z;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Octave accumulation: more octaves → finer detail, output still in [0,1]
// ---------------------------------------------------------------------------
TEST(PerlinNoiseTest, OctaveAccumulation)
{
    PerlinNoise perlin{99};
    double x = 3.7, y = 5.2;

    double v1 = perlin.Octave2D_01(x, y, 1);
    double v4 = perlin.Octave2D_01(x, y, 4);

    ASSERT_GE(v1, 0.0);
    ASSERT_LE(v1, 1.0);
    ASSERT_GE(v4, 0.0);
    ASSERT_LE(v4, 1.0);

    // With more octaves, the value should generally differ due to higher-frequency contribution
    // (not necessarily larger/smaller, just different)
    // We only verify range here; the statistical test below validates distribution.
}

// ---------------------------------------------------------------------------
// Statistical distribution: mean should be approximately 0.5
// ---------------------------------------------------------------------------
TEST(PerlinNoiseTest, StatisticalDistribution2D)
{
    PerlinNoise perlin{314};
    double sum = 0.0;
    constexpr int N = 10000;
    for (int i = 0; i < N; ++i) {
        double x = (i % 100) * 0.073;
        double y = (i / 100) * 0.073;
        sum += perlin.Octave2D_01(x, y, 3);
    }
    double mean = sum / N;
    ASSERT_NEAR(mean, 0.5, 0.1) << "Mean should be close to 0.5, got " << mean;
}

// ---------------------------------------------------------------------------
// Performance benchmark: 256x256 2D octave noise
// ---------------------------------------------------------------------------
TEST(PerlinNoiseTest, Benchmark2D_256x256)
{
    PerlinNoise perlin{42};
    constexpr uint32_t extent = 256;
    constexpr int octaves = 4;

    volatile double sink = 0;
    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < extent; ++i) {
        for (uint32_t j = 0; j < extent; ++j) {
            sink = perlin.Octave2D_01(i * 0.01, j * 0.01, octaves);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    printf("  [PERF] 2D 256x256 oct=%d: %lld us (%.2f ns/sample)\n",
           octaves, static_cast<long long>(ms), static_cast<double>(ms) * 1000.0 / (extent * extent));
}

// ---------------------------------------------------------------------------
// Performance benchmark: 64x64x64 3D octave noise
// ---------------------------------------------------------------------------
TEST(PerlinNoiseTest, Benchmark3D_64x64x64)
{
    PerlinNoise perlin{42};
    constexpr uint32_t extent = 64;
    constexpr int octaves = 4;

    volatile double sink = 0;
    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < extent; ++i) {
        for (uint32_t j = 0; j < extent; ++j) {
            for (uint32_t k = 0; k < extent; ++k) {
                sink = perlin.Octave3D_01(i * 0.01, j * 0.01, k * 0.01, octaves);
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    printf("  [PERF] 3D 64^3 oct=%d: %lld us (%.2f ns/sample)\n",
           octaves, static_cast<long long>(ms), static_cast<double>(ms) * 1000.0 / (extent * extent * extent));
}
