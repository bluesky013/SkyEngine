//
// Created on 2026/09/22.
//

#include <terrain/physics/TerrainCollisionLayer.h>

#include <terrain/TerrainSystem.h>
#include <terrain/TerrainRegion.h>

#include <physics/IPhysicsBackend.h>

#include <core/async/Task.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::terrain;

namespace {

    // Minimal engine-side fake: records created bodies and destroyed handles without a backend.
    class FakePhysicsWorld : public phy::IPhysicsWorld {
    public:
        phy::PhysicsBackendCaps            caps;
        std::vector<phy::PhysicsBodyDesc>  created;
        std::vector<phy::PhysicsObjectId>  destroyed;
        uint64_t                           next = 1;

        const phy::PhysicsBackendCaps &GetCaps() const override { return caps; }
        void           SetOptions(const phy::PhysicsOptions &) override {}
        phy::PhysicsOptions GetOptions() const override { return {}; }

        phy::PhysicsMaterialId CreateMaterial(const phy::PhysicsMaterialData &) override { return {}; }
        bool DestroyMaterial(phy::PhysicsMaterialId) override { return false; }
        bool GetMaterial(phy::PhysicsMaterialId, phy::PhysicsMaterialData &) const override { return false; }

        phy::PhysicsObjectId CreateBody(const phy::PhysicsBodyDesc &desc) override
        {
            created.push_back(desc);
            return phy::PhysicsObjectId{next++, 1};
        }
        bool DestroyObject(phy::PhysicsObjectId id) override
        {
            destroyed.push_back(id);
            return true;
        }
        bool HasObject(phy::PhysicsObjectId) const override { return false; }

        bool GetBodyTransform(phy::PhysicsObjectId, Transform &) const override { return false; }
        bool SetBodyTransform(phy::PhysicsObjectId, const Transform &) override { return false; }
        void SetInterpolationAlpha(float) override {}
        bool GetInterpolatedTransform(phy::PhysicsObjectId, Transform &) const override { return false; }
        bool GetBodyVelocity(phy::PhysicsObjectId, Vector3 &, Vector3 &) const override { return false; }
        bool SetBodyVelocity(phy::PhysicsObjectId, const Vector3 &, const Vector3 &) override { return false; }
        bool ApplyForce(phy::PhysicsObjectId, const Vector3 &, const Vector3 &) override { return false; }
        bool ApplyImpulse(phy::PhysicsObjectId, const Vector3 &, const Vector3 &) override { return false; }
        bool SetBodyFilter(phy::PhysicsObjectId, const phy::CollisionFilter &) override { return false; }
        bool SetBodyMaterial(phy::PhysicsObjectId, phy::PhysicsMaterialId) override { return false; }
        bool Wake(phy::PhysicsObjectId) override { return false; }

        phy::PhysicsObjectId CreateConstraint(const phy::ConstraintDesc &) override { return {}; }
        bool DestroyConstraint(phy::PhysicsObjectId) override { return false; }

        phy::PhysicsObjectId CreateCharacter(const phy::CharacterDesc &) override { return {}; }
        bool DestroyCharacter(phy::PhysicsObjectId) override { return false; }
        phy::CharacterMoveResult MoveCharacter(phy::PhysicsObjectId, const Vector3 &) override
        {
            return phy::CharacterMoveResult::NotAttached;
        }
        bool GetCharacterState(phy::PhysicsObjectId, phy::CharacterState &) const override { return false; }
        bool SetCharacterTransform(phy::PhysicsObjectId, const Transform &) override { return false; }
        bool SetCharacterCapsule(phy::PhysicsObjectId, float, float) override { return false; }

        void Step(float) override {}

        bool Raycast(const Vector3 &, const Vector3 &, float, const phy::PhysicsQueryFilter &, phy::PhysicsQueryHit &) const override { return false; }
        bool Sweep(const phy::ShapeDesc &, const Transform &, const Vector3 &, float, const phy::PhysicsQueryFilter &, phy::PhysicsQueryHit &) const override { return false; }
        void Overlap(const phy::ShapeDesc &, const Transform &, const phy::PhysicsQueryFilter &, std::vector<phy::PhysicsQueryOverlap> &) const override {}

        void DrainEvents(std::vector<phy::PhysicsEvent> &) override {}
        void CollectDebugGeometry(uint32_t, phy::PhysicsDebugGeometry &) const override {}
        phy::PhysicsWorldStats GetStats() const override { return {}; }
        bool CaptureState(phy::PhysicsWorldState &, phy::PhysicsSnapshotScope) const override { return false; }
        bool RestoreState(const phy::PhysicsWorldState &) override { return false; }
    };

    TerrainAssetData MakeSingleTileData()
    {
        TerrainAssetData data;
        data.meta.tileSize     = 8;
        data.meta.resolution   = 1.f;
        data.meta.heightFormat = TerrainHeightFormat::R32_SFLOAT;
        data.meta.heightScale  = 1.f;
        data.meta.heightOffset = 0.f;
        data.meta.lodCount     = 1;

        TerrainTilePayload tile;
        tile.coord = TerrainTileCoord{0, 0};
        TerrainLodPayload lod;
        lod.height.resize(static_cast<size_t>(data.meta.GetLodVertexCount(0)) * sizeof(float), 0);
        tile.lods.push_back(std::move(lod));
        data.tiles.push_back(std::move(tile));
        return data;
    }

    void Settle(TerrainSystem &system)
    {
        for (int i = 0; i < 64; ++i) {
            system.Tick(0.f);
            TaskExecutor::Get()->WaitForAll();
            system.Tick(0.f);
            if (system.GetPendingLoadCount() == 0 && system.GetLoadedTileCount() > 0) {
                break;
            }
        }
    }

} // namespace

TEST(TerrainCollisionLayerTest, BuildHeightFieldAndTransform)
{
    TerrainMeta meta;
    meta.tileSize   = 8;
    meta.resolution = 2.f;

    std::vector<float> heights(81);
    for (size_t i = 0; i < heights.size(); ++i) {
        heights[i] = static_cast<float>(i % 9);
    }

    const auto shape = TerrainCollisionLayer::BuildHeightField(meta, heights.data(), 9);
    EXPECT_EQ(shape.type, phy::ShapeType::HeightField);
    EXPECT_EQ(shape.cols, 9u);
    EXPECT_EQ(shape.rows, 9u);
    EXPECT_EQ(shape.samples.size(), 81u);
    EXPECT_FLOAT_EQ(shape.scaleX, 2.f);
    EXPECT_FLOAT_EQ(shape.scaleZ, 2.f);
    EXPECT_FLOAT_EQ(shape.minHeight, 0.f);
    EXPECT_FLOAT_EQ(shape.maxHeight, 8.f);
    EXPECT_EQ(shape.upAxis, 1);

    const auto transform = TerrainCollisionLayer::TileCollisionTransform(meta, TerrainTileCoord{1, 2});
    // tileWorld = 8 * 2 = 16; origin = (16, 0, 32); center = (24, 0, 40).
    EXPECT_FLOAT_EQ(transform.translation.x, 24.f);
    EXPECT_FLOAT_EQ(transform.translation.y, 0.f);
    EXPECT_FLOAT_EQ(transform.translation.z, 40.f);
}

TEST(TerrainCollisionLayerTest, CreatesAndDestroysColliders)
{
    const auto data = MakeSingleTileData();

    TerrainSystem system;
    ASSERT_TRUE(system.Setup(data));
    system.SetStreamingEnabled(true);
    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    system.SetStreamingRadii(12.f, 20.f);
    system.SetLoadBudget(10);
    Settle(system);
    ASSERT_GT(system.GetLoadedTileCount(), 0u);

    FakePhysicsWorld world;
    TerrainCollisionLayer layer;
    layer.Setup(&system, &world);

    layer.Update();
    EXPECT_EQ(layer.GetColliderCount(), 1u);
    ASSERT_EQ(world.created.size(), 1u);
    EXPECT_EQ(world.created[0].kind, phy::BodyKind::Static);
    EXPECT_EQ(world.created[0].shape.type, phy::ShapeType::HeightField);
    EXPECT_EQ(world.created[0].shape.cols, data.meta.GetTileVertexSize());

    system.SetStreamingFocus(Vector3(1000.f, 0.f, 1000.f));
    Settle(system);
    layer.Update();

    EXPECT_EQ(layer.GetColliderCount(), 0u);
    EXPECT_EQ(world.destroyed.size(), 1u);
}
