//
// Created on 2026/09/22.
//

#include <terrain/physics/TerrainCollisionLayer.h>

#include <terrain/TerrainSystem.h>
#include <terrain/TerrainRegion.h>

#include <physics/CollisionObject.h>
#include <physics/PhysicsRegistry.h>
#include <physics/PhysicsWorld.h>

#include <core/async/Task.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::terrain;

namespace {

    class FakeShape : public phy::IShapeImpl {
    public:
        CounterPtr<TriangleMesh> GetTriangleMesh() const override { return nullptr; }
    };

    class FakeCollisionObject : public phy::CollisionObject {
    public:
        void SetWorldTransform(const Transform &trans) override { transform = trans; }
        phy::PhysicsWorld *GetWorld() const override { return nullptr; }

        Transform transform;

    protected:
        void OnShapeChanged() override {}
        void OnGroupMaskChanged() override {}
    };

    class FakeWorld : public phy::PhysicsWorld {
    public:
        std::vector<phy::CollisionObject *> added;
        std::vector<phy::CollisionObject *> removed;

    protected:
        void AddRigidBodyImpl(phy::RigidBody *rb) override {}
        void RemoveRigidBodyImpl(phy::RigidBody *rb) override {}
        void AddCollisionObjectImpl(phy::CollisionObject *obj) override { added.push_back(obj); }
        void RemoveCollisionObjectImpl(phy::CollisionObject *obj) override { removed.push_back(obj); }
        void AddCharacterControllerImpl(phy::CharacterController *ch) override {}
        void RemoveCharacterControllerImpl(phy::CharacterController *ch) override {}
    };

    class FakeFactory : public phy::PhysicsRegistry::Impl {
    public:
        phy::PhysicsWorld *CreatePhysicsWorld() override { return nullptr; }
        phy::CollisionObject *CreateCollisionObject() override { return new FakeCollisionObject(); }
        phy::RigidBody *CreateRigidBody() override { return nullptr; }
        phy::CharacterController *CreateCharacterController() override { return nullptr; }

        phy::IShapeImpl *CreateBox(const phy::BoxShape &) override { return new FakeShape(); }
        phy::IShapeImpl *CreateSphere(const phy::SphereShape &) override { return new FakeShape(); }
        phy::IShapeImpl *CreateTriangleMesh(const phy::TriangleMeshShape &) override { return new FakeShape(); }
        phy::IShapeImpl *CreateHeightField(const phy::HeightFieldShape &shape) override
        {
            lastShape = shape;
            return new FakeShape();
        }
        phy::IShapeImpl *CreateCapsule(const phy::CapsuleShape &) override { return new FakeShape(); }
        phy::IMaterialImpl *CreateMaterial(const phy::PhysicsMaterialData &) override { return nullptr; }

        phy::HeightFieldShape lastShape;
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
    EXPECT_EQ(shape.width, 9u);
    EXPECT_EQ(shape.height, 9u);
    EXPECT_EQ(shape.samples.size(), 81u);
    EXPECT_FLOAT_EQ(shape.scaleX, 2.f);
    EXPECT_FLOAT_EQ(shape.scaleZ, 2.f);
    EXPECT_FLOAT_EQ(shape.minHeight, 0.f);
    EXPECT_FLOAT_EQ(shape.maxHeight, 8.f);
    EXPECT_EQ(shape.upAxis, 1);
    EXPECT_FALSE(shape.samples.empty());

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

    auto *factory = new FakeFactory();
    phy::PhysicsRegistry::Get()->Register(factory);

    FakeWorld world;
    TerrainCollisionLayer layer;
    layer.Setup(&system, &world);

    layer.Update();
    EXPECT_EQ(layer.GetColliderCount(), 1u);
    ASSERT_EQ(world.added.size(), 1u);
    EXPECT_EQ(factory->lastShape.width, data.meta.GetTileVertexSize());

    system.SetStreamingFocus(Vector3(1000.f, 0.f, 1000.f));
    Settle(system);
    layer.Update();

    EXPECT_EQ(layer.GetColliderCount(), 0u);
    EXPECT_EQ(world.removed.size(), 1u);

    phy::PhysicsRegistry::Get()->UnRegister();
}
