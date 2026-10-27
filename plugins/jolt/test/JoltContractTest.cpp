//
// Created on 2026/10/27.
//

#include "JoltBackend.h"
#include <framework/serialization/JsonArchive.h>
#include <gtest/gtest.h>
#include <jolt/PhysicsSubSystemConfig.h>
#include <jolt/PhysicsSystem.h>
#include <jolt/components/PhysicsBodyComponent.h>
#include <physics/PhysicsBackendRegistry.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>

using namespace sky;
using namespace sky::phy;

namespace {
    class JoltContractTest : public testing::Test {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(backend.Init());
            PhysicsWorldDesc desc;
            desc.workerCount = 0;
            world.reset(backend.CreateWorld(desc));
            ASSERT_NE(world, nullptr);
        }
        PhysicsObjectId Body(Vector3 position, BodyKind kind = BodyKind::Dynamic, CollisionFilter filter = {})
        {
            PhysicsBodyDesc desc;
            desc.kind                  = kind;
            desc.transform.translation = position;
            desc.shape.halfExt         = {0.5f, 0.5f, 0.5f};
            desc.filter                = filter;
            return world->CreateBody(desc);
        }
        void Steps(int count)
        {
            for (int i = 0; i < count; ++i) {
                ASSERT_TRUE(world->Step(1.f / 60.f)) << world->GetLastStatus().message;
            }
        }
        JoltBackend                    backend;
        std::unique_ptr<IPhysicsWorld> world;
    };
} // namespace

TEST_F(JoltContractTest, GenerationTypeAndWorldOwnership)
{
    auto body = Body({0, 3, 0});
    ASSERT_TRUE(IsValid(body));
    EXPECT_FALSE(world->DestroyCharacter(body));
    ASSERT_TRUE(world->DestroyObject(body));
    auto next = Body({0, 3, 0});
    EXPECT_EQ(body.index, next.index);
    EXPECT_NE(body.generation, next.generation);
    EXPECT_FALSE(world->SetBodyFilter(body, {}));
    std::unique_ptr<IPhysicsWorld> other(backend.CreateWorld({}));
    ASSERT_NE(other, nullptr);
    auto foreign = other->CreateBody({});
    EXPECT_FALSE(world->HasObject(foreign));
    ConstraintDesc joint;
    joint.bodyA = next;
    joint.bodyB = body;
    EXPECT_FALSE(IsValid(world->CreateConstraint(joint)));
    joint.bodyB = foreign;
    EXPECT_FALSE(IsValid(world->CreateConstraint(joint)));
}

TEST_F(JoltContractTest, NativeCapacityAndOptionFailureAreTransactional)
{
    PhysicsWorldDesc desc;
    desc.limits.bodies = 1;
    desc.workerCount   = 0;
    world.reset(backend.CreateWorld(desc));
    ASSERT_NE(world, nullptr);
    auto first = Body({0, 0, 0});
    ASSERT_TRUE(IsValid(first));
    EXPECT_FALSE(IsValid(Body({2, 0, 0})));
    EXPECT_EQ(world->GetLastStatus().error, PhysicsError::CapacityExceeded);
    auto options                     = world->GetOptions();
    options.allowDynamicTriangleMesh = true;
    EXPECT_FALSE(world->SetOptions(options));
    EXPECT_FALSE(world->GetOptions().allowDynamicTriangleMesh);
    EXPECT_TRUE(world->HasObject(first));
    CharacterDesc character;
    EXPECT_FALSE(IsValid(world->CreateCharacter(character)));
    EXPECT_TRUE(world->DestroyObject(first));
    EXPECT_TRUE(IsValid(world->CreateCharacter(character)));
}

TEST_F(JoltContractTest, BackendUnregisterAndRestartKeepOldWorldAlive)
{
    auto &registry = PhysicsBackendRegistry::Get();
    ASSERT_TRUE(registry.Register(std::make_unique<JoltBackend>()));
    auto *old = registry.CreateWorld({});
    ASSERT_NE(old, nullptr);
    auto body = old->CreateBody({});
    ASSERT_TRUE(IsValid(body));
    registry.Unregister();
    EXPECT_EQ(registry.CreateWorld({}), nullptr);
    ASSERT_TRUE(old->Step(1.f / 60.f));
    ASSERT_TRUE(registry.Register(std::make_unique<JoltBackend>()));
    ASSERT_TRUE(old->HasObject(body));
    registry.DestroyWorld(old);
    registry.Unregister();
    backend.Shutdown();
    EXPECT_EQ(backend.CreateWorld({}), nullptr);
    EXPECT_TRUE(backend.Init());
    ASSERT_TRUE(world->Step(1.f / 60.f));
}

TEST_F(JoltContractTest, PivotScaleAndQueriesUseObjectOrigin)
{
    PhysicsBodyDesc desc;
    desc.transform.translation = {10, 0, 0};
    desc.transform.scale       = {2, 2, 2};
    desc.shape.halfExt         = {1, 1, 1};
    desc.shape.pivot           = {2, 0, 0};
    auto body                  = world->CreateBody(desc);
    ASSERT_TRUE(IsValid(body));
    Transform out;
    ASSERT_TRUE(world->GetBodyTransform(body, out));
    EXPECT_FLOAT_EQ(out.translation.x, 10);
    PhysicsQueryHit hit;
    ASSERT_TRUE(world->Raycast({20, 0, 0}, {-2, 0, 0}, 20, {}, hit));
    EXPECT_NEAR(hit.position.x, 16, 0.01f);
    ShapeDesc sphere;
    sphere.type   = ShapeType::Sphere;
    sphere.radius = 0.5f;
    ASSERT_TRUE(world->Sweep(sphere, Transform{{20, 0, 0}}, {-1, 0, 0}, 20, {}, hit));
    EXPECT_NEAR(hit.distance, 3.5f, 0.02f);
    std::vector<PhysicsQueryOverlap> overlaps;
    world->Overlap(sphere, Transform{{14, 0, 0}}, {}, overlaps);
    ASSERT_EQ(overlaps.size(), 1u);
    EXPECT_EQ(overlaps[0].object, body);
    PhysicsQueryFilter filter;
    filter.ignoreExtra.push_back(body);
    EXPECT_FALSE(world->Raycast({20, 0, 0}, {-1, 0, 0}, 20, filter, hit));
    EXPECT_FALSE(world->Raycast({20, 0, 0}, {}, 20, {}, hit));
    EXPECT_EQ(world->GetLastStatus().error, PhysicsError::InvalidArgument);
}

TEST_F(JoltContractTest, CompoundOverlapIsDeduplicatedAndBadChildRollsBack)
{
    PhysicsBodyDesc desc;
    desc.shape.type = ShapeType::Compound;
    ShapeDesc child;
    child.halfExt                = {1, 1, 1};
    desc.shape.children          = {child, child};
    desc.shape.children[1].pivot = {1, 0, 0};
    auto body                    = world->CreateBody(desc);
    ASSERT_TRUE(IsValid(body));
    std::vector<PhysicsQueryOverlap> overlaps;
    ShapeDesc                        query;
    query.halfExt = {4, 4, 4};
    world->Overlap(query, {}, {}, overlaps);
    ASSERT_EQ(overlaps.size(), 1u);
    desc.shape.children[1].radius = -1;
    desc.shape.children[1].type   = ShapeType::Sphere;
    EXPECT_FALSE(IsValid(world->CreateBody(desc)));
    EXPECT_EQ(world->GetStats().bodyCount, 1u);
    PhysicsBodyDesc hull;
    hull.shape.type   = ShapeType::ConvexHull;
    hull.shape.points = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    EXPECT_TRUE(IsValid(world->CreateBody(hull)));
    hull.shape.points[3].x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(IsValid(world->CreateBody(hull)));
}

TEST_F(JoltContractTest, HeightfieldMappingUpAxisAndTileSeam)
{
    for (uint8_t axis = 0; axis < 3; ++axis) {
        PhysicsBodyDesc tile;
        tile.shape.type            = ShapeType::HeightField;
        tile.shape.cols            = 3;
        tile.shape.rows            = 2;
        tile.shape.samples         = {1, 1, 1, 1, 1, 1};
        tile.shape.heightScale     = 2;
        tile.shape.heightOffset    = 3;
        tile.shape.upAxis          = axis;
        tile.transform.translation = {float(axis) * 20, 0, 0};
        auto first                 = world->CreateBody(tile);
        ASSERT_TRUE(IsValid(first));
        Vector3 up{};
        up[axis] = 1;
        PhysicsQueryHit hit;
        ASSERT_TRUE(world->Raycast(tile.transform.translation + up * 10, -up, 20, {}, hit));
        EXPECT_NEAR(hit.position[axis] - tile.transform.translation[axis], 5, 0.01f);
        if (axis == 1) {
            tile.transform.translation.x += 2;
            auto second = world->CreateBody(tile);
            ASSERT_TRUE(IsValid(second));
            for (float x : {20.99f, 21.01f}) {
                ASSERT_TRUE(world->Raycast({x, 10, 0}, {0, -1, 0}, 20, {}, hit));
                EXPECT_NEAR(hit.position.y, 5, 0.01f);
            }
        }
    }
}

TEST_F(JoltContractTest, MeshIndexWidthsStrideAndMalformedData)
{
    for (auto type : {IndexType::U16, IndexType::U32}) {
        PhysicsBodyDesc desc;
        desc.shape.type        = ShapeType::TriangleMesh;
        auto *mesh             = new TriangleMesh();
        desc.shape.meshData    = mesh;
        mesh->vtxStride        = 16;
        mesh->indexType        = type;
        const float vertices[] = {-2, 0, -2, 99, 0, 0, 2, 99, 2, 0, -2, 99};
        mesh->position.resize(sizeof(vertices));
        std::memcpy(mesh->position.data(), vertices, sizeof(vertices));
        const uint16_t shortIndices[] = {0, 1, 2};
        const uint32_t indices[]      = {0, 1, 2};
        const size_t   bytes          = type == IndexType::U16 ? sizeof(shortIndices) : sizeof(indices);
        mesh->indexRaw.resize(bytes);
        std::memcpy(mesh->indexRaw.data(), type == IndexType::U16 ? static_cast<const void *>(shortIndices) : static_cast<const void *>(indices),
                    bytes);
        auto body = world->CreateBody(desc);
        ASSERT_TRUE(IsValid(body));
        PhysicsQueryHit hit;
        ASSERT_TRUE(world->Raycast({0, 3, 0}, {0, -1, 0}, 10, {}, hit));
        ASSERT_TRUE(world->DestroyObject(body));
        mesh->indexRaw[0] = 100;
        EXPECT_FALSE(IsValid(world->CreateBody(desc)));
    }
}

TEST_F(JoltContractTest, HighBitFiltersAndSleepingContactRemoval)
{
    const uint32_t high   = 1u << 31;
    auto           ground = Body({0, -0.5f, 0}, BodyKind::Static, {high, high});
    auto           body   = Body({0, 1, 0}, BodyKind::Dynamic, {high, high});
    ASSERT_TRUE(IsValid(ground));
    ASSERT_TRUE(IsValid(body));
    Steps(240);
    Transform transform;
    ASSERT_TRUE(world->GetBodyTransform(body, transform));
    EXPECT_NEAR(transform.translation.y, 0.5f, 0.05f);
    std::vector<PhysicsEvent> events;
    world->DrainEvents(events);
    EXPECT_EQ(std::count_if(events.begin(), events.end(), [](auto &e) { return e.type == PhysicsEventType::ContactBegin; }), 1);
    EXPECT_EQ(std::count_if(events.begin(), events.end(), [](auto &e) { return e.type == PhysicsEventType::ContactEnd; }), 0);
    EXPECT_EQ(world->GetStats().activeBodyCount, 0u);
    ASSERT_TRUE(world->SetBodyFilter(ground, {high, 0}));
    Steps(1);
    world->DrainEvents(events);
    EXPECT_EQ(std::count_if(events.begin(), events.end(), [](auto &e) { return e.type == PhysicsEventType::ContactEnd; }), 1);
    ASSERT_TRUE(world->Wake(body));
    Steps(30);
    ASSERT_TRUE(world->GetBodyTransform(body, transform));
    EXPECT_LT(transform.translation.y, 0);
}

TEST_F(JoltContractTest, MaterialsRejectUnsupportedValuesAndTrackOwnership)
{
    PhysicsMaterialData m;
    m.staticFriction = 0.2f;
    EXPECT_FALSE(IsValid(world->CreateMaterial(m)));
    EXPECT_EQ(world->GetLastStatus().error, PhysicsError::Unsupported);
    m.staticFriction = m.dynamicFriction = 0.2f;
    m.linearDamping                      = 1;
    auto material                        = world->CreateMaterial(m);
    ASSERT_TRUE(IsValid(material));
    PhysicsBodyDesc d;
    d.kind           = BodyKind::Dynamic;
    d.gravityEnabled = false;
    d.material       = material;
    auto body        = world->CreateBody(d);
    ASSERT_TRUE(IsValid(body));
    EXPECT_FALSE(world->DestroyMaterial(material));
    ASSERT_TRUE(world->SetBodyVelocity(body, {10, 0, 0}, {}));
    Steps(60);
    Vector3 linear, angular;
    ASSERT_TRUE(world->GetBodyVelocity(body, linear, angular));
    EXPECT_LT(linear.x, 5);
    ASSERT_TRUE(world->SetBodyMaterial(body, {}));
    EXPECT_TRUE(world->DestroyMaterial(material));
    EXPECT_FALSE(world->SetBodyMaterial(body, material));
    d.material      = {};
    d.linearDamping = {1, 2, 1};
    EXPECT_FALSE(IsValid(world->CreateBody(d)));
}

TEST_F(JoltContractTest, AllConstraintKindsAndDependentTeardown)
{
    for (auto type : {ConstraintType::Fixed, ConstraintType::Hinge, ConstraintType::Slider, ConstraintType::Distance, ConstraintType::Generic6DOF}) {
        auto           a = Body({0, 3, 0});
        auto           b = Body({0, 5, 0});
        ConstraintDesc d;
        d.type     = type;
        d.bodyA    = a;
        d.bodyB    = b;
        d.distance = 2;
        d.pivotA   = {0, 1, 0};
        d.pivotB   = {0, -1, 0};
        if (type == ConstraintType::Hinge) {
            d.angularX     = {true, -0.5f, 0.5f};
            d.angularMotor = {true, 1, 0, 0, 20};
        }
        if (type == ConstraintType::Slider) {
            d.linearX     = {true, -1, 1};
            d.linearMotor = {true, 1, 0, 20, 0};
        }
        if (type == ConstraintType::Generic6DOF) {
            d.linearX     = {true, -1, 1};
            d.angularX    = {true, -0.5f, 0.5f};
            d.linearMotor = {true, 0, 0.5f, 20, 0, PhysicsMotorMode::Position};
        }
        auto constraint = world->CreateConstraint(d);
        ASSERT_TRUE(IsValid(constraint)) << int(type) << world->GetLastStatus().message;
        Steps(2);
        ASSERT_TRUE(world->DestroyObject(a));
        EXPECT_FALSE(world->HasObject(constraint));
        ASSERT_TRUE(world->DestroyObject(b));
    }
    EXPECT_EQ(world->GetStats().constraintCount, 0u);
}

TEST_F(JoltContractTest, DistanceConstraintMaintainsNonzeroLength)
{
    auto           a = Body({0, 5, 0});
    auto           b = Body({2, 5, 0});
    ConstraintDesc d;
    d.type     = ConstraintType::Distance;
    d.bodyA    = a;
    d.bodyB    = b;
    d.distance = 2;
    ASSERT_TRUE(IsValid(world->CreateConstraint(d)));
    ASSERT_TRUE(world->ApplyImpulse(b, {0, 5, 0}, {2, 5, 0}));
    Steps(90);
    Transform ta, tb;
    ASSERT_TRUE(world->GetBodyTransform(a, ta));
    ASSERT_TRUE(world->GetBodyTransform(b, tb));
    EXPECT_NEAR((ta.translation - tb.translation).Length(), 2, 0.05f);
}

TEST_F(JoltContractTest, CharacterDisplacementGravityProxyAndResize)
{
    PhysicsBodyDesc ground;
    ground.shape.halfExt         = {10, 0.5f, 10};
    ground.transform.translation = {0, -0.5f, 0};
    ASSERT_TRUE(IsValid(world->CreateBody(ground)));
    CharacterDesc d;
    d.transform.translation = {0, 3, 0};
    auto character          = world->CreateCharacter(d);
    ASSERT_TRUE(IsValid(character));
    Steps(90);
    CharacterState state;
    ASSERT_TRUE(world->GetCharacterState(character, state));
    EXPECT_TRUE(state.grounded);
    ASSERT_EQ(world->MoveCharacter(character, {1, 0, 0}), CharacterMoveResult::Applied);
    Steps(1);
    ASSERT_TRUE(world->GetCharacterState(character, state));
    float x = state.transform.translation.x;
    Steps(10);
    ASSERT_TRUE(world->GetCharacterState(character, state));
    EXPECT_NEAR(state.transform.translation.x, x, 0.02f);
    PhysicsQueryHit hit;
    ASSERT_TRUE(world->Raycast({x, 5, 0}, {0, -1, 0}, 10, {}, hit));
    EXPECT_EQ(hit.object, character);
    EXPECT_FALSE(world->SetCharacterCapsule(character, -1, 1));
    EXPECT_TRUE(world->SetCharacterCapsule(character, 0.3f, 1));
    d.transform.translation = {x + 0.7f, state.transform.translation.y, 0};
    auto other              = world->CreateCharacter(d);
    ASSERT_TRUE(IsValid(other));
    ASSERT_EQ(world->MoveCharacter(character, {1, 0, 0}), CharacterMoveResult::Applied);
    Steps(1);
    ASSERT_TRUE(world->GetCharacterState(character, state));
    EXPECT_LT(state.transform.translation.x, x + 0.6f);
}

TEST_F(JoltContractTest, SnapshotValidationDoesNotPartiallyMutate)
{
    auto              a = Body({0, 4, 0});
    auto              b = Body({2, 4, 0});
    PhysicsWorldState state;
    ASSERT_TRUE(world->CaptureState(state));
    Steps(10);
    Transform before;
    ASSERT_TRUE(world->GetBodyTransform(a, before));
    state.bodies[0].transform.translation.y = 100;
    state.bodies[1].id.generation++;
    EXPECT_FALSE(world->RestoreState(state));
    Transform after;
    ASSERT_TRUE(world->GetBodyTransform(a, after));
    EXPECT_FLOAT_EQ(before.translation.y, after.translation.y);
    EXPECT_FALSE(world->Step(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_EQ(world->GetStats().stepCount, 10u);
    PhysicsDebugGeometry geometry;
    world->CollectDebugGeometry(PhysicsDebugCategory::Shapes | PhysicsDebugCategory::AABBs, geometry);
    EXPECT_FALSE(geometry.IsEmpty());
}

TEST_F(JoltContractTest, NativeUpdateFailureStopsPublicationAndFurtherSteps)
{
    PhysicsWorldDesc d;
    d.workerCount               = 0;
    d.limits.bodyPairs          = 1;
    d.limits.contactConstraints = 1;
    world.reset(backend.CreateWorld(d));
    ASSERT_NE(world, nullptr);
    for (int i = 0; i < 12; ++i) {
        ASSERT_TRUE(IsValid(Body({float(i % 3) * 0.3f, float(i / 3) * 0.3f, 0})));
    }
    auto status = world->Step(1.f / 60.f);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.error, PhysicsError::CapacityExceeded);
    EXPECT_EQ(world->GetStats().stepCount, 0u);
    std::vector<PhysicsEvent> events;
    world->DrainEvents(events);
    EXPECT_TRUE(events.empty());
    EXPECT_FALSE(world->Step(1.f / 60.f));
}

TEST_F(JoltContractTest, ScaleRecomputesInertiaAndDebugCategoriesStaySeparate)
{
    auto body = Body({0, 4, 0});
    ASSERT_TRUE(IsValid(body));
    ASSERT_TRUE(world->ApplyImpulse(body, {0, 1, 0}, {1, 4, 0}));
    Vector3 linear, angular;
    ASSERT_TRUE(world->GetBodyVelocity(body, linear, angular));
    const float initialAngular = angular.z;
    ASSERT_TRUE(world->SetBodyVelocity(body, VEC3_ZERO, VEC3_ZERO));
    Transform transform;
    ASSERT_TRUE(world->GetBodyTransform(body, transform));
    transform.scale = {2, 2, 2};
    ASSERT_TRUE(world->SetBodyTransform(body, transform));
    ASSERT_TRUE(world->ApplyImpulse(body, {0, 1, 0}, {1, 4, 0}));
    ASSERT_TRUE(world->GetBodyVelocity(body, linear, angular));
    EXPECT_NEAR(linear.y, 1.f, 1e-5f);
    EXPECT_NEAR(angular.z, initialAngular / 4.f, 1e-5f);

    PhysicsDebugGeometry geometry;
    world->CollectDebugGeometry(PhysicsDebugCategory::Character, geometry);
    EXPECT_TRUE(geometry.IsEmpty());
    CharacterDesc character;
    character.transform.translation = {5, 4, 0};
    ASSERT_TRUE(IsValid(world->CreateCharacter(character)));
    world->CollectDebugGeometry(PhysicsDebugCategory::Character, geometry);
    EXPECT_FALSE(geometry.IsEmpty());
    for (const auto &vertex : geometry.lines) {
        EXPECT_GT(vertex.position.x, 3.f);
    }
    for (const auto &vertex : geometry.triangles) {
        EXPECT_GT(vertex.position.x, 3.f);
    }
    const auto characterTriangles = geometry.triangles.size();
    world->CollectDebugGeometry(PhysicsDebugCategory::Character | PhysicsDebugCategory::AABBs, geometry);
    EXPECT_EQ(geometry.triangles.size(), characterTriangles);
}

TEST(JoltSerializationTest, CollectionsAndLegacyIdentityRoundTrip)
{
    PhysicsBodyData data;
    data.kind       = BodyKind::Dynamic;
    data.mass       = 7;
    data.shape.type = ShapeType::Compound;
    ShapeDesc height;
    height.type    = ShapeType::HeightField;
    height.cols    = 2;
    height.rows    = 2;
    height.samples = {1, 2, 3, 4};
    ShapeDesc hull;
    hull.type           = ShapeType::ConvexHull;
    hull.points         = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    data.shape.children = {height, hull};
    std::stringstream stream;
    {
        OStreamArchive    output(stream);
        JsonOutputArchive archive(output);
        archive.SaveValueObject(data);
    }
    PhysicsBodyData restored;
    {
        IStreamArchive   input(stream);
        JsonInputArchive archive(input);
        archive.LoadValueObject(restored);
    }
    EXPECT_EQ(restored.mass, 7);
    ASSERT_EQ(restored.shape.children.size(), 2u);
    EXPECT_EQ(restored.shape.children[0].samples, height.samples);
    ASSERT_EQ(restored.shape.children[1].points.size(), 4u);
    EXPECT_EQ(restored.shape.children[1].points[1].x, 1);
    EXPECT_NE(SerializationContext::Get()->FindType("PhysicsBodyData"), nullptr);
    EXPECT_NE(SerializationContext::Get()->FindType("PhysicsSubSystemConfig"), nullptr);
    std::stringstream legacy;
    legacy
        << "{\"classId\":\"" << TypeInfo<PhysicsSubSystemConfig>::RegisteredId().ToString()
        << R"(","elements":{"gravity":-12,"fixedTimestep":0.02,"maxSubSteps":3,"solverIterations":8,"sleepThreshold":0.01,"collisionMargin":0.02,"deterministic":false,"continuousCollision":true}})";
    PhysicsSubSystemConfig config;
    IStreamArchive         input(legacy);
    JsonInputArchive       archive(input);
    archive.LoadValueObject(config);
    PhysicsWorldDesc  desc;
    PhysicsStepConfig step;
    ASSERT_TRUE(config.MakeSettings(desc, step));
    EXPECT_EQ(desc.gravity.y, -12);
    EXPECT_TRUE(desc.options.enableCCD);
    EXPECT_EQ(desc.solverIterations, 8u);
    EXPECT_FLOAT_EQ(step.fixedDelta, 0.02f);
    EXPECT_EQ(desc.limits.bodies, 65536u);
    config.deterministic = true;
    EXPECT_EQ(config.MakeSettings(desc, step).error, PhysicsError::Unsupported);
}

TEST_F(JoltContractTest, RestitutionCombineModesAreSymmetric)
{
    for (auto mode : {PhysicsCombineMode::Average, PhysicsCombineMode::Min, PhysicsCombineMode::Multiply, PhysicsCombineMode::Max}) {
        for (bool swap : {false, true}) {
            PhysicsMaterialData a, b;
            a.staticFriction = a.dynamicFriction = b.staticFriction = b.dynamicFriction = 0;
            a.restitution                                                               = 0.2f;
            b.restitution                                                               = 0.8f;
            a.restitutionCombine                                                        = mode;
            auto            ma                                                          = world->CreateMaterial(a);
            auto            mb                                                          = world->CreateMaterial(b);
            PhysicsBodyDesc ground;
            ground.shape.halfExt         = {5, 0.5f, 5};
            ground.transform.translation = {0, -0.5f, 0};
            ground.material              = swap ? mb : ma;
            auto            gid          = world->CreateBody(ground);
            PhysicsBodyDesc sphere;
            sphere.kind                  = BodyKind::Dynamic;
            sphere.gravityEnabled        = false;
            sphere.shape.type            = ShapeType::Sphere;
            sphere.shape.radius          = 0.5f;
            sphere.transform.translation = {0, 0.5f, 0};
            sphere.material              = swap ? ma : mb;
            auto sid                     = world->CreateBody(sphere);
            ASSERT_TRUE(IsValid(sid));
            ASSERT_TRUE(world->SetBodyVelocity(sid, {0, -10, 0}, {}));
            ASSERT_TRUE(world->Step(0.01f));
            Vector3 linear, angular;
            ASSERT_TRUE(world->GetBodyVelocity(sid, linear, angular));
            float expected = mode == PhysicsCombineMode::Average    ? 5
                             : mode == PhysicsCombineMode::Min      ? 2
                             : mode == PhysicsCombineMode::Multiply ? 1.6f
                                                                    : 8;
            EXPECT_NEAR(linear.y, expected, 0.3f) << int(mode) << " swap=" << swap;
            ASSERT_TRUE(world->DestroyObject(sid));
            ASSERT_TRUE(world->DestroyObject(gid));
        }
    }
}

TEST_F(JoltContractTest, CcdGatePreventsThinWallTunneling)
{
    for (bool enabled : {false, true}) {
        PhysicsWorldDesc wd;
        wd.workerCount       = 0;
        wd.options.enableCCD = enabled;
        wd.gravity           = {};
        world.reset(backend.CreateWorld(wd));
        ASSERT_NE(world, nullptr);
        PhysicsBodyDesc wall;
        wall.shape.halfExt = {0.02f, 5, 5};
        ASSERT_TRUE(IsValid(world->CreateBody(wall)));
        PhysicsBodyDesc sphere;
        sphere.kind                  = BodyKind::Dynamic;
        sphere.enableCCD             = true;
        sphere.shape.type            = ShapeType::Sphere;
        sphere.shape.radius          = 0.1f;
        sphere.transform.translation = {-2, 0, 0};
        auto id                      = world->CreateBody(sphere);
        ASSERT_TRUE(IsValid(id));
        ASSERT_TRUE(world->SetBodyVelocity(id, {300, 0, 0}, {}));
        Steps(1);
        Transform result;
        ASSERT_TRUE(world->GetBodyTransform(id, result));
        if (enabled) {
            EXPECT_LT(result.translation.x, 0);
        } else {
            EXPECT_GT(result.translation.x, 0);
        }
    }
}

TEST_F(JoltContractTest, SliderMotorMovesAndHonorsLimit)
{
    PhysicsBodyDesc body;
    body.kind           = BodyKind::Dynamic;
    body.gravityEnabled = false;
    auto id             = world->CreateBody(body);
    ASSERT_TRUE(IsValid(id));
    ConstraintDesc d;
    d.type  = ConstraintType::Slider;
    d.bodyA = id;
    d.axisA = d.axisB = {1, 0, 0};
    d.linearX         = {true, -1, 1};
    d.linearMotor     = {true, 2, 0, 20, 0};
    ASSERT_TRUE(IsValid(world->CreateConstraint(d)));
    Steps(90);
    Transform transform;
    ASSERT_TRUE(world->GetBodyTransform(id, transform));
    EXPECT_GT(std::abs(transform.translation.x), 0.5f);
    EXPECT_LT(std::abs(transform.translation.x), 1.05f);
}

TEST_F(JoltContractTest, TickPauseAndSingleStepPublishEvents)
{
    auto &registry = PhysicsBackendRegistry::Get();
    ASSERT_TRUE(registry.Register(std::make_unique<JoltBackend>()));
    PhysicsSystem system;
    ASSERT_TRUE(system.Init());
    system.SetStepConfig({0.01f, 4});
    PhysicsBodyDesc trigger;
    trigger.isTrigger = true;
    auto tid          = system.GetWorld()->CreateBody(trigger);
    ASSERT_TRUE(IsValid(tid));
    PhysicsBodyDesc body;
    body.kind           = BodyKind::Dynamic;
    body.gravityEnabled = false;
    ASSERT_TRUE(IsValid(system.GetWorld()->CreateBody(body)));
    system.SetSimulating(false);
    system.Tick(1);
    EXPECT_EQ(system.GetStats().stepCount, 0u);
    system.SingleStep();
    EXPECT_EQ(system.GetStats().stepCount, 1u);
    std::vector<PhysicsEvent> events;
    system.DrainEvents(events);
    EXPECT_FALSE(events.empty());
    system.SetSimulating(true);
    system.Tick(0.02f);
    EXPECT_EQ(system.GetStats().stepCount, 3u);
    registry.Unregister();
}

TEST_F(JoltContractTest, CharacterStairsSlopeAndMovingPlatform)
{
    PhysicsBodyDesc floor;
    floor.shape.halfExt         = {10, 0.5f, 10};
    floor.transform.translation = {0, -0.5f, 0};
    auto floorId                = world->CreateBody(floor);
    ASSERT_TRUE(IsValid(floorId));
    PhysicsBodyDesc step;
    step.shape.halfExt         = {1, 0.15f, 3};
    step.transform.translation = {1.5f, 0.15f, 0};
    ASSERT_TRUE(IsValid(world->CreateBody(step)));
    CharacterDesc character;
    character.transform.translation = {-1, 1.5f, 0};
    auto id                         = world->CreateCharacter(character);
    ASSERT_TRUE(IsValid(id));
    Steps(60);
    for (int i = 0; i < 90; ++i) {
        ASSERT_EQ(world->MoveCharacter(id, {0.03f, 0, 0}), CharacterMoveResult::Applied);
        Steps(1);
    }
    CharacterState state;
    ASSERT_TRUE(world->GetCharacterState(id, state));
    EXPECT_GT(state.transform.translation.x, 1);
    EXPECT_GT(state.transform.translation.y, 1.3f);
    PhysicsWorldDesc wd;
    wd.workerCount = 0;
    world.reset(backend.CreateWorld(wd));
    PhysicsBodyDesc slope;
    slope.shape.halfExt      = {5, 0.5f, 5};
    slope.transform.rotation = Quaternion(0.25f, {0, 0, 1});
    ASSERT_TRUE(IsValid(world->CreateBody(slope)));
    character.transform.translation = {0, 3, 0};
    id                              = world->CreateCharacter(character);
    ASSERT_TRUE(IsValid(id));
    Steps(120);
    ASSERT_TRUE(world->GetCharacterState(id, state));
    EXPECT_TRUE(state.grounded);
    world.reset(backend.CreateWorld(wd));
    PhysicsBodyDesc platform;
    platform.kind          = BodyKind::Kinematic;
    platform.shape.halfExt = {5, 0.5f, 5};
    auto pid               = world->CreateBody(platform);
    ASSERT_TRUE(IsValid(pid));
    character.transform.translation = {0, 3, 0};
    id                              = world->CreateCharacter(character);
    Steps(120);
    ASSERT_TRUE(world->SetBodyVelocity(pid, {1, 0, 0}, {}));
    Steps(60);
    ASSERT_TRUE(world->GetCharacterState(id, state));
    EXPECT_GT(state.transform.translation.x, 0.7f);
}
