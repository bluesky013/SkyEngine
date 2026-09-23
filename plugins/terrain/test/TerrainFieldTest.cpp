//
// Created on 2026/09/22.
//

#include <terrain/TerrainField.h>

#include <gtest/gtest.h>

#include <functional>

using namespace sky;
using namespace sky::terrain;

namespace {

    TerrainMeta MakeMeta()
    {
        TerrainMeta meta;
        meta.tileSize     = 4;
        meta.resolution   = 1.f;
        meta.heightFormat = TerrainHeightFormat::R32_SFLOAT;
        meta.heightScale  = 1.f;
        meta.heightOffset = 0.f;
        meta.lodCount     = 3;
        return meta;
    }

    TerrainLodPayload MakeHeightPayload(const TerrainMeta &meta, const std::function<float(float, float)> &fn)
    {
        TerrainLodPayload lod;
        const uint32_t size = meta.GetTileVertexSize();
        lod.height.resize(static_cast<size_t>(meta.GetTileVertexCount()) * sizeof(float));
        auto *samples = reinterpret_cast<float *>(lod.height.data());
        for (uint32_t z = 0; z < size; ++z) {
            for (uint32_t x = 0; x < size; ++x) {
                samples[static_cast<size_t>(z) * size + x] = fn(static_cast<float>(x), static_cast<float>(z));
            }
        }
        return lod;
    }

} // namespace

TEST(TerrainFieldTest, QueryHeightBilinear)
{
    const TerrainMeta meta = MakeMeta();

    TerrainField field;
    field.SetMeta(meta);
    ASSERT_TRUE(field.AddTile({0, 0}, MakeHeightPayload(meta, [](float x, float z) { return x + 2.f * z; })));

    float height = 0.f;
    ASSERT_TRUE(field.QueryHeight(Vector3(1.5f, 0.f, 2.5f), height));
    EXPECT_FLOAT_EQ(height, 6.5f);
}

TEST(TerrainFieldTest, QueryHeightUnloadedTileFails)
{
    const TerrainMeta meta = MakeMeta();
    TerrainField field;
    field.SetMeta(meta);

    float height = 0.f;
    EXPECT_FALSE(field.QueryHeight(Vector3(1.f, 0.f, 1.f), height));
}

TEST(TerrainFieldTest, QueryNormalFromGradient)
{
    const TerrainMeta meta = MakeMeta();

    TerrainField field;
    field.SetMeta(meta);
    ASSERT_TRUE(field.AddTile({0, 0}, MakeHeightPayload(meta, [](float x, float z) { return x + 2.f * z; })));

    Vector3 normal;
    ASSERT_TRUE(field.QueryNormal(Vector3(1.5f, 0.f, 2.5f), normal));

    Vector3 expected(-1.f, 1.f, -2.f);
    expected.Normalize();
    EXPECT_NEAR(normal.x, expected.x, 1e-4f);
    EXPECT_NEAR(normal.y, expected.y, 1e-4f);
    EXPECT_NEAR(normal.z, expected.z, 1e-4f);
}

TEST(TerrainFieldTest, QuerySplatWeights)
{
    const TerrainMeta meta = MakeMeta();

    TerrainField field;
    field.SetMeta(meta);

    auto payload = MakeHeightPayload(meta, [](float, float) { return 0.f; });
    const uint32_t size = meta.GetTileVertexSize();
    payload.splat.push_back(std::vector<uint8_t>(static_cast<size_t>(size) * size * 4u, 0));

    const size_t base = (static_cast<size_t>(2) * size + 1) * 4u;
    payload.splat[0][base + 0] = 255;
    payload.splat[0][base + 1] = 128;
    payload.splat[0][base + 2] = 0;
    payload.splat[0][base + 3] = 64;
    ASSERT_TRUE(field.AddTile({0, 0}, payload));

    Vector4 weights;
    ASSERT_TRUE(field.QuerySplatWeights(Vector3(1.5f, 0.f, 2.5f), weights));
    EXPECT_FLOAT_EQ(weights.x, 1.f);
    EXPECT_FLOAT_EQ(weights.y, 128.f / 255.f);
    EXPECT_FLOAT_EQ(weights.z, 0.f);
    EXPECT_FLOAT_EQ(weights.w, 64.f / 255.f);
}

TEST(TerrainFieldTest, RaycastHitAndMiss)
{
    const TerrainMeta meta = MakeMeta();

    TerrainField field;
    field.SetMeta(meta);
    ASSERT_TRUE(field.AddTile({0, 0}, MakeHeightPayload(meta, [](float, float) { return 0.f; })));

    TerrainRaycastHit hit;
    ASSERT_TRUE(field.Raycast(Vector3(2.f, 10.f, 2.f), Vector3(0.f, -1.f, 0.f), 20.f, hit));
    EXPECT_NEAR(hit.distance, 10.f, 0.1f);
    EXPECT_NEAR(hit.position.y, 0.f, 0.1f);

    TerrainRaycastHit miss;
    EXPECT_FALSE(field.Raycast(Vector3(2.f, 10.f, 2.f), Vector3(1.f, 0.f, 0.f), 20.f, miss));
}
