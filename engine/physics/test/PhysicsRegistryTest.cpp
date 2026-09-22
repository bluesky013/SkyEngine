//
// Created on 2026/09/22.
//

#include <physics/PhysicsRegistry.h>
#include <physics/PhysicsWorld.h>
#include <physics/PhysicsDebugGeometry.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::phy;

namespace {

    class MockImpl : public PhysicsRegistry::Impl {
    public:
        bool Init() override
        {
            initCalled = true;
            return true;
        }

        void Shutdown() override { shutdownCalled = true; }

        PhysicsWorld* CreatePhysicsWorld() override { return nullptr; }
        CollisionObject* CreateCollisionObject() override { return nullptr; }
        RigidBody* CreateRigidBody() override { return nullptr; }
        CharacterController* CreateCharacterController() override { return nullptr; }

        IShapeImpl* CreateBox(const BoxShape &shape) override
        {
            boxCalls++;
            return nullptr;
        }

        IShapeImpl* CreateSphere(const SphereShape &shape) override
        {
            sphereCalls++;
            return nullptr;
        }

        IShapeImpl* CreateTriangleMesh(const TriangleMeshShape &shape) override
        {
            triangleMeshCalls++;
            return nullptr;
        }

        IShapeImpl* CreateHeightField(const HeightFieldShape &shape) override
        {
            heightFieldCalls++;
            lastHeightField = shape;
            return nullptr;
        }

        IShapeImpl* CreateCapsule(const CapsuleShape &shape) override
        {
            capsuleCalls++;
            lastCapsule = shape;
            return nullptr;
        }

        IMaterialImpl* CreateMaterial(const PhysicsMaterialData &data) override
        {
            materialCalls++;
            lastMaterial = data;
            return nullptr;
        }

        bool  initCalled     = false;
        bool  shutdownCalled = false;
        int   boxCalls = 0;
        int   sphereCalls = 0;
        int   triangleMeshCalls = 0;
        int   heightFieldCalls = 0;
        int   capsuleCalls = 0;
        int   materialCalls = 0;
        HeightFieldShape lastHeightField;
        CapsuleShape     lastCapsule;
        PhysicsMaterialData lastMaterial;
    };

} // namespace

TEST(PhysicsRegistryTest, LifecycleOrdering)
{
    auto *impl = new MockImpl();
    auto *registry = PhysicsRegistry::Get();

    registry->Register(impl);
    EXPECT_TRUE(impl->initCalled);
    EXPECT_FALSE(impl->shutdownCalled);

    registry->UnRegister();
    EXPECT_TRUE(impl->shutdownCalled);
}

TEST(PhysicsRegistryTest, GuardsAbsentBackend)
{
    auto *registry = PhysicsRegistry::Get();
    registry->UnRegister();

    EXPECT_EQ(registry->CreatePhysicsWorld(), nullptr);
    EXPECT_EQ(registry->CreateBox(BoxShape{}), nullptr);
}

TEST(PhysicsRegistryTest, ForwardsShapeCreation)
{
    auto *impl = new MockImpl();
    auto *registry = PhysicsRegistry::Get();
    registry->Register(impl);

    HeightFieldShape heightField;
    heightField.width  = 5;
    heightField.height = 7;
    heightField.samples.assign(35, 0.f);
    registry->CreateHeightField(heightField);

    CapsuleShape capsule;
    capsule.radius = 0.5f;
    capsule.height = 2.f;
    registry->CreateCapsule(capsule);

    PhysicsMaterialData materialData;
    materialData.staticFriction  = 0.7f;
    materialData.dynamicFriction = 0.6f;
    materialData.restitution     = 0.2f;
    registry->CreateMaterial(materialData);

    EXPECT_EQ(impl->heightFieldCalls, 1);
    EXPECT_EQ(impl->lastHeightField.width, 5u);
    EXPECT_EQ(impl->lastHeightField.height, 7u);
    EXPECT_EQ(impl->capsuleCalls, 1);
    EXPECT_FLOAT_EQ(impl->lastCapsule.radius, 0.5f);
    EXPECT_EQ(impl->materialCalls, 1);
    EXPECT_FLOAT_EQ(impl->lastMaterial.staticFriction, 0.7f);
    EXPECT_FLOAT_EQ(impl->lastMaterial.restitution, 0.2f);

    registry->UnRegister();
}

TEST(PhysicsMaterialTest, Defaults)
{
    const PhysicsMaterialData def = GetDefaultPhysicsMaterial();
    EXPECT_FLOAT_EQ(def.staticFriction, 0.5f);
    EXPECT_FLOAT_EQ(def.dynamicFriction, 0.5f);
    EXPECT_FLOAT_EQ(def.restitution, 0.f);
}

namespace {

    class MockCharacter : public CharacterController {
    };

} // namespace

TEST(CharacterControllerTest, MinimalContractDefaults)
{
    MockCharacter controller;
    EXPECT_FALSE(controller.Move(Vector3(1.f, 0.f, 0.f)));
    EXPECT_FALSE(controller.IsGrounded());

    const Transform transform = controller.GetWorldTransform();
    EXPECT_FLOAT_EQ(transform.translation.x, 0.f);
    EXPECT_FLOAT_EQ(transform.translation.y, 0.f);
    EXPECT_FLOAT_EQ(transform.translation.z, 0.f);
}

TEST(PhysicsDebugGeometryTest, PlainGeometry)
{
    PhysicsDebugGeometry geometry;
    const float color[4] = {1.f, 0.f, 0.f, 1.f};

    geometry.AddLine(VEC3_ZERO, VEC3_Y, color);
    geometry.AddTriangle(VEC3_ZERO, Vector3(1.f, 0.f, 0.f), VEC3_Y, color);

    EXPECT_EQ(geometry.lines.size(), 2u);
    EXPECT_EQ(geometry.triangles.size(), 3u);
    EXPECT_FALSE(geometry.IsEmpty());
    EXPECT_FLOAT_EQ(geometry.lines[1].position.y, 1.f);
    EXPECT_FLOAT_EQ(geometry.lines[0].color[0], 1.f);

    geometry.Clear();
    EXPECT_TRUE(geometry.IsEmpty());
}

namespace {

    class MockWorld : public PhysicsWorld {
    public:
        bool overrideRaycast = false;

        bool Raycast(const Vector3 &origin, const Vector3 &dir, float maxDistance, RaycastHit &out,
                     const CollisionFilters *filter) const override
        {
            if (!overrideRaycast) {
                return PhysicsWorld::Raycast(origin, dir, maxDistance, out, filter);
            }
            out.position = Vector3(1.f, 2.f, 3.f);
            out.normal   = VEC3_Y;
            out.distance = 5.f;
            return true;
        }

    protected:
        void AddRigidBodyImpl(RigidBody *) override {}
        void RemoveRigidBodyImpl(RigidBody *) override {}
        void AddCollisionObjectImpl(CollisionObject *) override {}
        void RemoveCollisionObjectImpl(CollisionObject *) override {}
        void AddCharacterControllerImpl(CharacterController *) override {}
        void RemoveCharacterControllerImpl(CharacterController *) override {}
        void Tick(float) override {}
        void OnAttachToWorld(World &) override {}
        void OnDetachFromWorld(World &) override {}
    };

} // namespace

TEST(PhysicsQueryTest, DefaultRaycastReportsNoHit)
{
    MockWorld world;
    RaycastHit hit;
    EXPECT_FALSE(world.Raycast(VEC3_ZERO, VEC3_Y, 100.f, hit, nullptr));
    EXPECT_EQ(hit.object, nullptr);
}

TEST(PhysicsQueryTest, RaycastResultIsBackendNeutral)
{
    MockWorld world;
    world.overrideRaycast = true;

    RaycastHit hit;
    ASSERT_TRUE(world.Raycast(VEC3_ZERO, VEC3_Y, 100.f, hit, nullptr));
    EXPECT_FLOAT_EQ(hit.position.y, 2.f);
    EXPECT_FLOAT_EQ(hit.distance, 5.f);
}

TEST(PhysicsFilterTest, GroupMaskContract)
{
    const CollisionFilters all  = CollisionFilterBit::ALL;
    const CollisionFilters none = CollisionFilters(static_cast<CollisionFilters::ValueType>(0));

    EXPECT_TRUE(CollisionFilterAccepts(CollisionFilterBit::DEFAULT, all, CollisionFilterBit::DEFAULT, all));
    EXPECT_FALSE(CollisionFilterAccepts(CollisionFilterBit::DEFAULT, none, CollisionFilterBit::DEFAULT, all));
    EXPECT_FALSE(CollisionFilterAccepts(CollisionFilterBit::DEFAULT, all, CollisionFilterBit::DEFAULT, none));

    // DFT_STATIC does not interact with a mask that only allows characters.
    const CollisionFilters characterMask = CollisionFilterBit::DFT_CHARACTER;
    EXPECT_FALSE(CollisionFilterAccepts(CollisionFilterBit::DFT_STATIC, all, CollisionFilterBit::DFT_CHARACTER, characterMask));
    EXPECT_TRUE(CollisionFilterAccepts(CollisionFilterBit::DFT_CHARACTER, all, CollisionFilterBit::DFT_CHARACTER, all));
}
