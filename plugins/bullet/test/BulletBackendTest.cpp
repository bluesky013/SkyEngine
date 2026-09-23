//
// Created on 2026/09/23.
//

#include <bullet/BulletBackend.h>
#include <bullet/PhysicsSystem.h>
#include <bullet/components/PhysicsBodyComponent.h>

#include <physics/PhysicsBackendRegistry.h>

#include <framework/serialization/SerializationContext.h>
#include <framework/world/Actor.h>
#include <framework/world/World.h>

#include <gtest/gtest.h>

#include <memory>

using namespace sky;
using namespace sky::phy;

namespace {

    ShapeDesc MakeBox(const Vector3 &halfExt)
    {
        ShapeDesc shape;
        shape.type    = ShapeType::Box;
        shape.halfExt = halfExt;
        return shape;
    }

    ShapeDesc MakeSphere(float radius)
    {
        ShapeDesc shape;
        shape.type   = ShapeType::Sphere;
        shape.radius = radius;
        return shape;
    }

    PhysicsBodyDesc MakeStaticBody(const ShapeDesc &shape, const Transform &transform, uint32_t group = 1)
    {
        PhysicsBodyDesc body;
        body.kind      = BodyKind::Static;
        body.shape     = shape;
        body.transform = transform;
        body.filter    = CollisionFilter{group, 0xFFFFFFFF};
        return body;
    }

    PhysicsBodyDesc MakeDynamicSphere(float radius, const Transform &transform, uint32_t group = 2)
    {
        PhysicsBodyDesc body;
        body.kind      = BodyKind::Dynamic;
        body.shape     = MakeSphere(radius);
        body.mass      = 1.f;
        body.transform = transform;
        body.filter    = CollisionFilter{group, 0xFFFFFFFF};
        return body;
    }

    void StepFor(IPhysicsWorld &world, int steps)
    {
        for (int i = 0; i < steps; ++i) {
            world.Step(1.f / 60.f);
        }
    }

} // namespace

TEST(BulletBackendTest, BodyStepAndTransform)
{
    BulletBackend backend;
    IPhysicsWorld *world = backend.CreateWorld(PhysicsWorldDesc{});
    ASSERT_NE(world, nullptr);

    const PhysicsObjectId ground = world->CreateBody(
        MakeStaticBody(MakeBox(Vector3(10.f, 0.5f, 10.f)), Transform{{0, -0.5f, 0}}));
    const PhysicsObjectId sphere = world->CreateBody(
        MakeDynamicSphere(0.5f, Transform{{0, 5.f, 0}}));
    ASSERT_TRUE(IsValid(ground));
    ASSERT_TRUE(IsValid(sphere));

    StepFor(*world, 180);

    Transform transform;
    ASSERT_TRUE(world->GetBodyTransform(sphere, transform));
    EXPECT_LT(transform.translation.y, 5.f);
    EXPECT_GT(transform.translation.y, 0.3f);
    EXPECT_LT(transform.translation.y, 0.8f);

    backend.DestroyWorld(world);
}

TEST(BulletBackendTest, QueryResolvesHandlesAndSkipsFiltered)
{
    BulletBackend backend;
    IPhysicsWorld *world = backend.CreateWorld(PhysicsWorldDesc{});
    ASSERT_NE(world, nullptr);

    const PhysicsObjectId ground = world->CreateBody(
        MakeStaticBody(MakeBox(Vector3(10.f, 0.5f, 10.f)), Transform{{0, -0.5f, 0}}, 1));
    const PhysicsObjectId sphere = world->CreateBody(
        MakeDynamicSphere(0.5f, Transform{{0, 3.f, 0}}, 2));

    PhysicsQueryHit hit;
    PhysicsQueryFilter filter;
    ASSERT_TRUE(world->Raycast(Vector3(0, 5, 0), Vector3(0, -1, 0), 20.f, filter, hit));
    EXPECT_EQ(hit.object, sphere);

    // Mask only accepts group 1 (ground): the closer sphere is skipped, the ground is returned.
    PhysicsQueryFilter groundOnly;
    groundOnly.mask = 1u;
    ASSERT_TRUE(world->Raycast(Vector3(0, 5, 0), Vector3(0, -1, 0), 20.f, groundOnly, hit));
    EXPECT_EQ(hit.object, ground);

    // Ignoring the sphere still returns the ground.
    PhysicsQueryFilter ignoreSphere;
    ignoreSphere.hasIgnore = true;
    ignoreSphere.ignore    = sphere;
    ASSERT_TRUE(world->Raycast(Vector3(0, 5, 0), Vector3(0, -1, 0), 20.f, ignoreSphere, hit));
    EXPECT_EQ(hit.object, ground);

    backend.DestroyWorld(world);
}

TEST(BulletBackendTest, SnapshotRoundTrip)
{
    BulletBackend backend;
    IPhysicsWorld *world = backend.CreateWorld(PhysicsWorldDesc{});
    ASSERT_NE(world, nullptr);

    world->CreateBody(MakeStaticBody(MakeBox(Vector3(10.f, 0.5f, 10.f)), Transform{{0, -0.5f, 0}}));
    const PhysicsObjectId sphere = world->CreateBody(MakeDynamicSphere(0.5f, Transform{{0, 3.f, 0}}));

    StepFor(*world, 30);
    PhysicsWorldState state;
    ASSERT_TRUE(world->CaptureState(state));
    ASSERT_FALSE(state.bodies.empty());

    Transform captured;
    world->GetBodyTransform(sphere, captured);

    StepFor(*world, 30);
    Transform moved;
    world->GetBodyTransform(sphere, moved);
    EXPECT_NE(captured.translation.y, moved.translation.y);

    ASSERT_TRUE(world->RestoreState(state));
    Transform restored;
    world->GetBodyTransform(sphere, restored);
    EXPECT_FLOAT_EQ(restored.translation.y, captured.translation.y);

    backend.DestroyWorld(world);
}

TEST(BulletBackendTest, TriggerEventsAreEmitted)
{
    BulletBackend backend;
    IPhysicsWorld *world = backend.CreateWorld(PhysicsWorldDesc{});
    ASSERT_NE(world, nullptr);

    PhysicsBodyDesc ground = MakeStaticBody(MakeBox(Vector3(10.f, 0.5f, 10.f)), Transform{{0, -0.5f, 0}});
    world->CreateBody(ground);

    // Bullet does not generate manifolds for static-static pairs, so a trigger needs at least one
    // non-static participant to be detected in this first implementation.
    PhysicsBodyDesc trigger = MakeStaticBody(MakeBox(Vector3(2.f, 2.f, 2.f)), Transform{{0, 1.f, 0}});
    trigger.isTrigger = true;
    world->CreateBody(trigger);

    world->CreateBody(MakeDynamicSphere(0.5f, Transform{{0, 1.f, 0}}));

    world->Step(1.f / 60.f);

    std::vector<PhysicsEvent> events;
    world->DrainEvents(events);
    bool foundTrigger = false;
    for (const auto &event : events) {
        if (event.type == PhysicsEventType::TriggerEnter && IsValid(event.a) && IsValid(event.b)) {
            foundTrigger = true;
        }
    }
    EXPECT_TRUE(foundTrigger);

    backend.DestroyWorld(world);
}

TEST(BulletBackendTest, CharacterAndTeardownAreSafe)
{
    IPhysicsWorld *world = nullptr;
    {
        BulletBackend backend;
        world = backend.CreateWorld(PhysicsWorldDesc{});
        ASSERT_NE(world, nullptr);

        world->CreateBody(MakeStaticBody(MakeBox(Vector3(10.f, 0.5f, 10.f)), Transform{{0, -0.5f, 0}}));

        CharacterDesc character;
        character.transform = Transform{{0, 2.f, 0}};
        const PhysicsObjectId id = world->CreateCharacter(character);
        ASSERT_TRUE(IsValid(id));

        StepFor(*world, 60);
        CharacterState state;
        ASSERT_TRUE(world->GetCharacterState(id, state));
        EXPECT_LT(state.transform.translation.y, 2.f);

        // Intentionally destroy the world while the character is still attached.
        backend.DestroyWorld(world);
    }
}

TEST(BulletBackendTest, StatsAndModeRejection)
{
    auto &registry = PhysicsBackendRegistry::Get();
    ASSERT_TRUE(registry.Register(std::make_unique<BulletBackend>()));

    PhysicsWorldDesc exact;
    exact.mathMode = PhysicsMathMode::Exact;
    EXPECT_EQ(registry.CreateWorld(exact), nullptr);

    PhysicsWorldDesc fast;
    fast.mathMode = PhysicsMathMode::Fast;
    IPhysicsWorld *world = registry.CreateWorld(fast);
    ASSERT_NE(world, nullptr);

    world->CreateBody(MakeStaticBody(MakeBox(Vector3(10.f, 0.5f, 10.f)), Transform{{0, -0.5f, 0}}));
    StepFor(*world, 10);

    const PhysicsWorldStats stats = world->GetStats();
    EXPECT_GT(stats.stepCount, 0u);
    EXPECT_EQ(stats.bodyCount, 1u);

    registry.DestroyWorld(world);
    registry.Unregister();
}

TEST(BulletBackendTest, OptionsAreConfigurable)
{
    BulletBackend backend;
    PhysicsWorldDesc desc;
    desc.options.allowDynamicTriangleMesh = false;
    desc.options.maxBodies                = 1;
    IPhysicsWorld *world = backend.CreateWorld(desc);
    ASSERT_NE(world, nullptr);

    EXPECT_TRUE(IsValid(world->CreateBody(
        MakeStaticBody(MakeBox(Vector3(1.f, 1.f, 1.f)), Transform{{0, 0, 0}}))));
    // maxBodies = 1 rejects the second body.
    EXPECT_FALSE(IsValid(world->CreateBody(
        MakeStaticBody(MakeBox(Vector3(1.f, 1.f, 1.f)), Transform{{2, 0, 0}}))));

    const PhysicsOptions options = world->GetOptions();
    EXPECT_FALSE(options.allowDynamicTriangleMesh);
    EXPECT_EQ(options.maxBodies, 1u);

    backend.DestroyWorld(world);
}

TEST(PhysicsComponentTest, AttachDetachAndReattach)
{
    PhysicsBodyComponent::Reflect(SerializationContext::Get());

    auto &registry = PhysicsBackendRegistry::Get();
    ASSERT_TRUE(registry.Register(std::make_unique<BulletBackend>()));

    std::unique_ptr<World> world(World::CreateWorld());
    world->Init();
    ASSERT_NE(AttachPhysicsSystem(*world), nullptr);

    auto actor = world->CreateActor("body");
    auto *body = actor->AddComponent<PhysicsBodyComponent>();
    ASSERT_NE(body, nullptr);
    EXPECT_TRUE(IsValid(body->GetObjectId()));

    world->DetachFromWorld(actor);
    EXPECT_FALSE(IsValid(body->GetObjectId()));

    world->AttachToWorld(actor);
    EXPECT_TRUE(IsValid(body->GetObjectId()));

    registry.Unregister();
}
