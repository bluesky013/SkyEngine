#include "JoltBackend.h"
#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/World.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <gtest/gtest.h>
#include <jolt/PhysicsSubSystemConfig.h>
#include <jolt/PhysicsSystem.h>
#include <jolt/components/PhysicsBodyComponent.h>
#include <physics/PhysicsBackendRegistry.h>
#include <physics/PhysicsMesh.h>

using namespace sky;
using namespace sky::phy;

class JoltIntegrationTest : public testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_TRUE(PhysicsBackendRegistry::Get().Register(std::make_unique<JoltBackend>()));
    }
    void TearDown() override
    {
        PhysicsBackendRegistry::Get().Unregister();
    }
};

TEST_F(JoltIntegrationTest, MovingStaticActorMovesItsCollider)
{
    std::unique_ptr<World> host(World::CreateWorld());
    auto                  *system = AttachPhysicsSystem(*host);
    ASSERT_NE(system, nullptr);
    auto *actor = host->CreateActor("Static");
    auto *body  = actor->AddComponent<PhysicsBodyComponent>();
    actor->GetComponent<TransformComponent>()->SetLocalTranslation({7, 8, 9});
    host->Tick(1.f / 60.f);
    Transform actual;
    ASSERT_TRUE(system->GetWorld()->GetBodyTransform(body->GetObjectId(), actual));
    EXPECT_FLOAT_EQ(actual.translation.x, 7.f);
}

TEST_F(JoltIntegrationTest, KinematicActorPoseIsNotOverwrittenByOldPhysicsPose)
{
    std::unique_ptr<World> host(World::CreateWorld());
    auto                  *system = AttachPhysicsSystem(*host);
    ASSERT_NE(system, nullptr);
    host->StartSimulation();
    auto *actor = host->CreateActor("Kinematic");
    auto *body  = actor->AddComponent<PhysicsBodyComponent>();
    body->SetKind(BodyKind::Kinematic);
    auto *transform = actor->GetComponent<TransformComponent>();
    transform->SetLocalTranslation({7, 8, 9});
    host->Tick(1.f / 60.f);
    EXPECT_FLOAT_EQ(transform->GetWorldTransform().translation.x, 7.f);
    Transform actual;
    ASSERT_TRUE(system->GetWorld()->GetBodyTransform(body->GetObjectId(), actual));
    EXPECT_NEAR(actual.translation.x, 7.f, .001f);
    host->Tick(1.f / 60.f);
    ASSERT_TRUE(system->GetWorld()->GetBodyTransform(body->GetObjectId(), actual));
    EXPECT_NEAR(actual.translation.x, 7.f, .001f);
}

TEST_F(JoltIntegrationTest, FailedShapeEditRetainsWorkingBody)
{
    std::unique_ptr<World> host(World::CreateWorld());
    auto                  *system = AttachPhysicsSystem(*host);
    ASSERT_NE(system, nullptr);
    auto      *body   = host->CreateActor("Body")->AddComponent<PhysicsBodyComponent>();
    const auto before = body->GetObjectId();
    ASSERT_TRUE(system->GetWorld()->HasObject(before));
    ShapeDesc invalid;
    invalid.type   = ShapeType::Sphere;
    invalid.radius = -1;
    EXPECT_FALSE(body->SetShape(invalid));
    EXPECT_EQ(body->GetObjectId(), before);
    EXPECT_TRUE(IsValid(body->GetObjectId()));
    EXPECT_TRUE(system->GetWorld()->HasObject(before));
}

TEST_F(JoltIntegrationTest, MassEditRetainsExistingConstraint)
{
    std::unique_ptr<World> host(World::CreateWorld());
    auto                  *system = AttachPhysicsSystem(*host);
    ASSERT_NE(system, nullptr);
    auto *a = host->CreateActor("A")->AddComponent<PhysicsBodyComponent>();
    auto *b = host->CreateActor("B")->AddComponent<PhysicsBodyComponent>();
    a->SetKind(BodyKind::Dynamic);
    b->SetKind(BodyKind::Dynamic);
    ConstraintDesc constraint;
    constraint.bodyA = a->GetObjectId();
    constraint.bodyB = b->GetObjectId();
    const auto id    = system->GetWorld()->CreateConstraint(constraint);
    ASSERT_TRUE(IsValid(id));
    const auto bodyId = a->GetObjectId();
    ASSERT_TRUE(system->GetWorld()->SetBodyVelocity(bodyId, {1, 2, 3}, {0, 1, 0}));
    ASSERT_TRUE(a->SetMass(2.f));
    EXPECT_EQ(a->GetObjectId(), bodyId);
    Vector3 linear, angular;
    ASSERT_TRUE(system->GetWorld()->GetBodyVelocity(bodyId, linear, angular));
    EXPECT_FLOAT_EQ(linear.y, 2.f);
    ShapeDesc edited;
    edited.halfExt = {2, 1, 1};
    edited.pivot   = {1, 0, 0};
    ASSERT_TRUE(a->SetShape(edited));
    EXPECT_EQ(a->GetObjectId(), bodyId);
    EXPECT_TRUE(system->GetWorld()->HasObject(id));
}

TEST_F(JoltIntegrationTest, CollisionOnlyMeshIsIncludedInAssetRoots)
{
    std::unique_ptr<World> host(World::CreateWorld());
    auto                  *body = host->CreateActor("Collider")->AddComponent<PhysicsBodyComponent>();
    ShapeDesc              shape;
    shape.type = ShapeType::TriangleMesh;
    shape.mesh = Uuid::Create();
    SetPhysicsMeshProvider({}, {}, "AuroraMesh");
    ShapeDesc compound;
    compound.type = ShapeType::Compound;
    compound.children.push_back(shape);
    ASSERT_TRUE(body->SetShape(compound));
    std::vector<ComponentAssetReference> references;
    body->CollectAssetReferences(references);
    ASSERT_EQ(references.size(), 1u);
    EXPECT_EQ(references.front().uuid, shape.mesh);
    EXPECT_EQ(references.front().type, "AuroraMesh");
}

TEST_F(JoltIntegrationTest, MeasureUnconsumedEventRetention)
{
    PhysicsSystem system;
    ASSERT_TRUE(system.Init({}));
    PhysicsBodyDesc ground;
    ground.transform.translation = {0, -1, 0};
    ground.shape.halfExt         = {10, 1, 10};
    ASSERT_TRUE(IsValid(system.GetWorld()->CreateBody(ground)));
    PhysicsBodyDesc box;
    box.kind                  = BodyKind::Dynamic;
    box.transform.translation = {0, 1, 0};
    ASSERT_TRUE(IsValid(system.GetWorld()->CreateBody(box)));
    for (int i = 0; i < 600; ++i)
        system.SingleStep();
    std::vector<PhysicsEvent> retained;
    system.DrainEvents(retained);
    EXPECT_LT(retained.size(), 16u);
}

TEST_F(JoltIntegrationTest, DynamicWritebackUsesTheCompletedPhysicsStep)
{
    std::unique_ptr<World> host(World::CreateWorld());
    auto                  *system = AttachPhysicsSystem(*host);
    ASSERT_NE(system, nullptr);
    auto *actor     = host->CreateActor("Dynamic");
    auto *transform = actor->GetComponent<TransformComponent>();
    transform->SetLocalTranslation({0, 5, 0});
    auto *body = actor->AddComponent<PhysicsBodyComponent>();
    ASSERT_TRUE(body->SetKind(BodyKind::Dynamic));
    host->StartSimulation();
    host->Tick(1.5f / 60.f);
    Transform rendered;
    ASSERT_TRUE(system->GetWorld()->GetInterpolatedTransform(body->GetObjectId(), rendered));
    EXPECT_LT(rendered.translation.y, 5.f);
    EXPECT_FLOAT_EQ(transform->GetWorldTransform().translation.y, rendered.translation.y);
    transform->SetLocalTranslation({0, 10, 0});
    Transform teleported;
    ASSERT_TRUE(system->GetWorld()->GetBodyTransform(body->GetObjectId(), teleported));
    EXPECT_FLOAT_EQ(teleported.translation.y, 10.f);
}

TEST_F(JoltIntegrationTest, MissingCollisionProductRollsBackWorldInitialization)
{
    WorldSubSystemRegistration registration;
    registration.factory = [](World &, const Any &) -> std::unique_ptr<IWorldSubSystem> {
        auto system = std::make_unique<PhysicsSystem>();
        if (!system->Init({}))
            return {};
        return system;
    };
    ASSERT_TRUE(WorldSubSystemRegistry::Get().Register(Name("Physics"), std::move(registration)));
    struct Cleanup {
        ~Cleanup()
        {
            WorldSubSystemRegistry::Get().Unregister(Name("Physics"));
        }
    } cleanup;
    std::unique_ptr<World> world(World::CreateWorld());
    auto                  *actor = world->CreateActor("Missing collision product");
    auto                  *body  = actor->AddComponent<PhysicsBodyComponent>();
    ShapeDesc              shape;
    shape.type = ShapeType::TriangleMesh;
    shape.mesh = Uuid::Create();
    ASSERT_TRUE(body->SetShape(shape));
    SetPhysicsMeshProvider({}, {}, "AuroraMesh");
    WorldDesc desc;
    desc.subSystems.push_back({Name("Physics"), {}, true});
    EXPECT_FALSE(world->Build(desc));
    EXPECT_NE(world->GetLastError().find("Collision mesh product"), std::string::npos);
    EXPECT_EQ(world->GetSubSystem(Name("Physics")), nullptr);
    EXPECT_EQ(actor->GetWorld(), nullptr);
    EXPECT_FALSE(IsValid(body->GetObjectId()));
    ASSERT_TRUE(body->SetShape({}));
    ASSERT_TRUE(world->Build(desc));
    EXPECT_TRUE(IsValid(body->GetObjectId()));
}

TEST_F(JoltIntegrationTest, AllListenersSeeOrderedFrameEventsAndPollingDoesNotReplayHistory)
{
    struct Listener : IPhysicsEventListener {
        std::vector<PhysicsEventType> types;
        void                          OnPhysicsEvents(const std::vector<PhysicsEvent> &events) override
        {
            for (const auto &event : events)
                types.push_back(event.type);
        }
    } first, second;
    PhysicsSystem system;
    ASSERT_TRUE(system.Init({}));
    system.AddEventListener(&first);
    system.AddEventListener(&second);
    PhysicsBodyDesc trigger;
    trigger.isTrigger = true;
    ASSERT_TRUE(IsValid(system.GetWorld()->CreateBody(trigger)));
    PhysicsBodyDesc body;
    body.kind           = BodyKind::Dynamic;
    body.gravityEnabled = false;
    const auto id       = system.GetWorld()->CreateBody(body);
    ASSERT_TRUE(IsValid(id));
    system.SingleStep();
    ASSERT_FALSE(first.types.empty());
    EXPECT_EQ(first.types.front(), PhysicsEventType::TriggerEnter);
    ASSERT_TRUE(system.GetWorld()->SetBodyTransform(id, Transform{{10, 0, 0}}));
    system.SingleStep();
    EXPECT_EQ(first.types, second.types);
    ASSERT_EQ(first.types.size(), 2u);
    EXPECT_EQ(first.types.back(), PhysicsEventType::TriggerExit);
    std::vector<PhysicsEvent> current;
    system.DrainEvents(current);
    ASSERT_EQ(current.size(), 1u);
    EXPECT_EQ(current.front().type, PhysicsEventType::TriggerExit);
    current.clear();
    system.SingleStep();
    system.DrainEvents(current);
    EXPECT_TRUE(current.empty());
    system.RemoveEventListener(&first);
    system.RemoveEventListener(&second);
}
