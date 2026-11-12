#include "JoltBackend.h"

#include <gtest/gtest.h>
#include <physics/softbody/ISoftBodyCollisionQuery.h>

#include <algorithm>
#include <limits>
#include <memory>

using namespace sky;
using namespace sky::phy;

namespace {
    class JoltSoftBodyCollisionTest : public testing::Test {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(backend.Init());
            PhysicsWorldDesc desc;
            desc.workerCount = 0;
            desc.gravity     = VEC3_ZERO;
            world.reset(backend.CreateWorld(desc));
            ASSERT_NE(world, nullptr);
            queryProvider = world->GetSoftBodyCollisionQuery();
            ASSERT_NE(queryProvider, nullptr);
        }

        PhysicsObjectId Sphere(Vector3 center, BodyKind kind = BodyKind::Static, CollisionFilter filter = {})
        {
            PhysicsBodyDesc body;
            body.kind                  = kind;
            body.transform.translation = center;
            body.shape.type            = ShapeType::Sphere;
            body.shape.radius          = 1.f;
            body.filter                = filter;
            return world->CreateBody(body);
        }

        PhysicsObjectId Box(Vector3 center, Vector3 extents)
        {
            PhysicsBodyDesc body;
            body.transform.translation = center;
            body.shape.halfExt         = extents;
            return world->CreateBody(body);
        }

        SoftBodyCollisionQuery Query(Vector3 start, Vector3 end) const
        {
            SoftBodyCollisionQuery query;
            query.start     = start;
            query.end       = end;
            query.radius    = .25f;
            query.deltaTime = DELTA;
            query.step      = world->GetStats().stepCount;
            return query;
        }

        void Step()
        {
            ASSERT_TRUE(world->Step(DELTA)) << world->GetLastStatus().message;
        }

        static constexpr float         DELTA = 1.f / 60.f;
        JoltBackend                    backend;
        std::unique_ptr<IPhysicsWorld> world;
        ISoftBodyCollisionQuery       *queryProvider = nullptr;
    };
} // namespace

TEST_F(JoltSoftBodyCollisionTest, StaticSphereSweepReturnsEndPlaneAndStableObject)
{
    const auto sphere = Sphere({0, 0, 0});
    ASSERT_TRUE(IsValid(sphere));
    Step();
    std::vector<SoftBodyCollisionContact> contacts;
    const auto                            status = queryProvider->QuerySoftBodyContacts(Query({-3, 0, 0}, {3, 0, 0}), contacts);
    ASSERT_TRUE(status) << status.message;
    ASSERT_EQ(contacts.size(), 1u);
    const auto &hit = contacts.front();
    EXPECT_EQ(hit.object, sphere);
    EXPECT_NEAR(hit.fraction, 1.75f / 6.f, 1e-4f);
    EXPECT_NEAR(hit.point.x, -1.f, 1e-4f);
    EXPECT_NEAR(hit.normal.x, -1.f, 1e-4f);
    EXPECT_EQ(hit.surfaceVelocity, VEC3_ZERO);
    EXPECT_FALSE(hit.dynamic);
}

TEST_F(JoltSoftBodyCollisionTest, InitialAndEndpointPenetrationBothReturnSupportingPlanes)
{
    ASSERT_TRUE(IsValid(Sphere({0, 0, 0})));
    Step();
    std::vector<SoftBodyCollisionContact> contacts;
    auto                                  status = queryProvider->QuerySoftBodyContacts(Query({1.1f, 0, 0}, {1.1f, 0, 0}), contacts);
    ASSERT_TRUE(status) << status.message;
    ASSERT_FALSE(contacts.empty());
    EXPECT_NEAR(contacts.front().point.x, 1.f, 1e-4f);
    EXPECT_NEAR(contacts.front().normal.x, 1.f, 1e-4f);
    EXPECT_FLOAT_EQ(contacts.front().fraction, 0.f);
    status = queryProvider->QuerySoftBodyContacts(Query({3, 0, 0}, {1.1f, 0, 0}), contacts);
    ASSERT_TRUE(status) << status.message;
    ASSERT_FALSE(contacts.empty());
    EXPECT_LT(contacts.front().fraction, 1.f);
    EXPECT_GT(contacts.front().fraction, 0.f);
    EXPECT_NEAR(contacts.front().normal.x, 1.f, 1e-4f);
}

TEST_F(JoltSoftBodyCollisionTest, CornerReturnsAllIndependentContacts)
{
    const auto floor = Box({0, -.5f, 0}, {10, .5f, 10});
    const auto wall  = Box({-.5f, 0, 0}, {.5f, 10, 10});
    ASSERT_TRUE(IsValid(floor));
    ASSERT_TRUE(IsValid(wall));
    Step();
    std::vector<SoftBodyCollisionContact> contacts;
    const auto                            status = queryProvider->QuerySoftBodyContacts(Query({1, 1, 0}, {.1f, .1f, 0}), contacts);
    ASSERT_TRUE(status) << status.message;
    const auto floorContact = std::find_if(contacts.begin(), contacts.end(), [&](const auto &hit) { return hit.object == floor; });
    const auto wallContact  = std::find_if(contacts.begin(), contacts.end(), [&](const auto &hit) { return hit.object == wall; });
    ASSERT_NE(floorContact, contacts.end());
    ASSERT_NE(wallContact, contacts.end());
    EXPECT_GT(floorContact->normal.y, .99f);
    EXPECT_GT(wallContact->normal.x, .99f);
}

TEST_F(JoltSoftBodyCollisionTest, DeepBoxEntryKeepsEntryPlaneInsteadOfOppositeTerminalExit)
{
    ASSERT_TRUE(IsValid(Box({0, 0, 0}, {.5f, .5f, .5f})));
    Step();
    auto query   = Query({-2, 0, 0}, {.4f, 0, 0});
    query.radius = .1f;
    std::vector<SoftBodyCollisionContact> contacts;
    const auto                            status = queryProvider->QuerySoftBodyContacts(query, contacts);
    ASSERT_TRUE(status) << status.message;
    ASSERT_FALSE(contacts.empty());
    for (const auto &hit : contacts) {
        EXPECT_LT(hit.normal.x, -.99f);
        EXPECT_NEAR(hit.point.x, -.5f, 1e-4f);
    }
}

TEST_F(JoltSoftBodyCollisionTest, ReciprocalFilterAndTriggersDoNotProducePhysicalContacts)
{
    const auto accepted = Sphere({0, 0, 0}, BodyKind::Static, {2, 1});
    ASSERT_TRUE(IsValid(accepted));
    ASSERT_TRUE(IsValid(Sphere({0, 0, 0}, BodyKind::Static, {2, 4})));
    PhysicsBodyDesc trigger;
    trigger.isTrigger = true;
    trigger.filter    = {2, 1};
    ASSERT_TRUE(IsValid(world->CreateBody(trigger)));
    Step();
    auto query   = Query({-3, 0, 0}, {3, 0, 0});
    query.filter = {1, 2};
    std::vector<SoftBodyCollisionContact> contacts;
    ASSERT_TRUE(queryProvider->QuerySoftBodyContacts(query, contacts));
    ASSERT_FALSE(contacts.empty());
    for (const auto &contact : contacts)
        EXPECT_EQ(contact.object, accepted);
    query.filter.mask = 0;
    ASSERT_TRUE(queryProvider->QuerySoftBodyContacts(query, contacts));
    EXPECT_TRUE(contacts.empty());
}

TEST_F(JoltSoftBodyCollisionTest, KinematicCrossingUsesFullMotionIntervalAndSurfaceVelocity)
{
    const auto moving = Sphere({-3, 0, 0}, BodyKind::Kinematic);
    ASSERT_TRUE(IsValid(moving));
    ASSERT_TRUE(world->SetKinematicTarget(moving, Transform{{3, 0, 0}}));
    Step();
    std::vector<SoftBodyCollisionContact> contacts;
    const auto                            status = queryProvider->QuerySoftBodyContacts(Query({0, 0, 0}, {0, 0, 0}), contacts);
    ASSERT_TRUE(status) << status.message;
    ASSERT_EQ(contacts.size(), 1u);
    const auto &hit = contacts.front();
    EXPECT_EQ(hit.object, moving);
    EXPECT_NEAR(hit.fraction, 1.75f / 6.f, 1e-4f);
    EXPECT_GT(hit.normal.x, .99f);
    EXPECT_NEAR(hit.point.x, 4.f, 1e-4f);
    EXPECT_NEAR(hit.surfaceVelocity.x, 6.f / DELTA, .01f);
    EXPECT_FALSE(hit.dynamic);
}

TEST_F(JoltSoftBodyCollisionTest, InvalidAndStaleQueriesPreserveOutput)
{
    std::vector<SoftBodyCollisionContact> contacts(1);
    contacts.front().point = {9, 8, 7};
    auto query             = Query({-3, 0, 0}, {3, 0, 0});
    EXPECT_EQ(queryProvider->QuerySoftBodyContacts(query, contacts).error, PhysicsError::InvalidState);
    Step();
    query        = Query({-3, 0, 0}, {3, 0, 0});
    query.radius = 0;
    EXPECT_EQ(queryProvider->QuerySoftBodyContacts(query, contacts).error, PhysicsError::InvalidArgument);
    query.radius  = .25f;
    query.start.x = std::numeric_limits<float>::infinity();
    EXPECT_EQ(queryProvider->QuerySoftBodyContacts(query, contacts).error, PhysicsError::InvalidArgument);
    query = Query({-3, 0, 0}, {3, 0, 0});
    query.deltaTime *= 2;
    EXPECT_EQ(queryProvider->QuerySoftBodyContacts(query, contacts).error, PhysicsError::InvalidState);
    query = Query({-3, 0, 0}, {3, 0, 0});
    Step();
    EXPECT_EQ(queryProvider->QuerySoftBodyContacts(query, contacts).error, PhysicsError::InvalidState);
    ASSERT_EQ(contacts.size(), 1u);
    EXPECT_EQ(contacts.front().point, Vector3(9, 8, 7));
}

TEST_F(JoltSoftBodyCollisionTest, ForeignWorldBodiesNeverLeakIntoQueryResults)
{
    std::unique_ptr<IPhysicsWorld> other(backend.CreateWorld({}));
    ASSERT_NE(other, nullptr);
    const auto foreign = other->CreateBody({});
    ASSERT_TRUE(IsValid(foreign));
    Step();
    std::vector<SoftBodyCollisionContact> contacts;
    ASSERT_TRUE(queryProvider->QuerySoftBodyContacts(Query({-3, 0, 0}, {3, 0, 0}), contacts));
    EXPECT_TRUE(contacts.empty());
    const auto own = Sphere({0, 0, 0});
    Step();
    ASSERT_TRUE(queryProvider->QuerySoftBodyContacts(Query({-3, 0, 0}, {3, 0, 0}), contacts));
    ASSERT_FALSE(contacts.empty());
    for (const auto &hit : contacts) {
        EXPECT_EQ(hit.object, own);
        EXPECT_NE(hit.object, foreign);
    }
}

TEST_F(JoltSoftBodyCollisionTest, FiniteInputsThatOverflowGeometryArithmeticPreserveOutput)
{
    Step();
    const float                         maximum = std::numeric_limits<float>::max();
    std::vector<SoftBodyCollisionQuery> queries;
    auto                                query = Query({0, 0, 0}, {1, 0, 0});
    query.radius                              = maximum;
    queries.push_back(query);
    queries.push_back(Query({-maximum, 0, 0}, {maximum, 0, 0}));
    queries.push_back(Query({maximum, 0, 0}, {maximum, 0, 0}));
    queries.push_back(Query({0, 0, 0}, {1e20f, 0, 0}));
    std::vector<SoftBodyCollisionContact> contacts(1);
    contacts.front().point = {9, 8, 7};
    for (const auto &invalid : queries) {
        const auto status = queryProvider->QuerySoftBodyContacts(invalid, contacts);
        EXPECT_EQ(status.error, PhysicsError::InvalidArgument) << status.message;
        ASSERT_EQ(contacts.size(), 1u);
        EXPECT_EQ(contacts.front().point, Vector3(9, 8, 7));
    }
    ASSERT_TRUE(queryProvider->QuerySoftBodyContacts(Query({0, 0, 0}, {1, 0, 0}), contacts));
    EXPECT_TRUE(contacts.empty());
}

TEST_F(JoltSoftBodyCollisionTest, SceneMutationsInvalidateCompletedIntervalUntilNextStep)
{
    const auto sphere = Sphere({0, 0, 0});
    ASSERT_TRUE(IsValid(sphere));
    Step();
    const auto staleAndResume = [&]() {
        std::vector<SoftBodyCollisionContact> contacts(1);
        contacts.front().point = {9, 8, 7};
        const auto status      = queryProvider->QuerySoftBodyContacts(Query({-3, 0, 0}, {3, 0, 0}), contacts);
        EXPECT_EQ(status.error, PhysicsError::InvalidState);
        ASSERT_EQ(contacts.size(), 1u);
        EXPECT_EQ(contacts.front().point, Vector3(9, 8, 7));
        Step();
        EXPECT_TRUE(queryProvider->QuerySoftBodyContacts(Query({-3, 0, 0}, {3, 0, 0}), contacts));
    };
    ASSERT_TRUE(world->SetBodyTransform(sphere, Transform{{.5f, 0, 0}}));
    staleAndResume();
    Transform scaled{{.5f, 0, 0}};
    scaled.scale = {.5f, .5f, .5f};
    ASSERT_TRUE(world->SetBodyTransform(sphere, scaled));
    staleAndResume();
    ShapeDesc shape;
    shape.halfExt = {.5f, .5f, .5f};
    ASSERT_TRUE(world->UpdateBody(sphere, BodyKind::Static, 1.f, shape));
    staleAndResume();
    ASSERT_TRUE(world->SetBodyFilter(sphere, {1, 2}));
    staleAndResume();
    const auto added = Sphere({0, 4, 0});
    ASSERT_TRUE(IsValid(added));
    staleAndResume();
    ASSERT_TRUE(world->DestroyObject(added));
    staleAndResume();
    PhysicsWorldState saved;
    ASSERT_TRUE(world->CaptureState(saved, PhysicsSnapshotScope::Full));
    ASSERT_TRUE(world->RestoreState(saved));
    staleAndResume();
    ASSERT_TRUE(world->BeginActivation(1));
    ASSERT_TRUE(IsValid(Sphere({0, 8, 0})));
    world->EndActivation(true);
    staleAndResume();
}

TEST_F(JoltSoftBodyCollisionTest, FailedMutationsAndUnpublishedRollbackPreserveCompletedInterval)
{
    const auto sphere = Sphere({0, 0, 0});
    ASSERT_TRUE(IsValid(sphere));
    Step();
    const auto query = Query({-3, 0, 0}, {3, 0, 0});
    Transform  invalid;
    invalid.scale = VEC3_ZERO;
    EXPECT_FALSE(world->SetBodyTransform(sphere, invalid));
    ShapeDesc invalidShape;
    invalidShape.type   = ShapeType::Sphere;
    invalidShape.radius = -1.f;
    EXPECT_FALSE(world->UpdateBody(sphere, BodyKind::Static, 1.f, invalidShape));
    EXPECT_FALSE(world->DestroyObject({}));
    PhysicsBodyDesc invalidBody;
    invalidBody.shape = invalidShape;
    EXPECT_FALSE(IsValid(world->CreateBody(invalidBody)));
    PhysicsWorldState invalidSnapshot;
    invalidSnapshot.bodies.emplace_back();
    EXPECT_FALSE(world->RestoreState(invalidSnapshot));
    ASSERT_TRUE(world->BeginActivation(1));
    ASSERT_TRUE(IsValid(Sphere({0, 4, 0})));
    world->EndActivation(false);
    std::vector<SoftBodyCollisionContact> contacts;
    const auto                            status = queryProvider->QuerySoftBodyContacts(query, contacts);
    ASSERT_TRUE(status) << status.message;
    ASSERT_EQ(contacts.size(), 1u);
    EXPECT_EQ(contacts.front().object, sphere);
}

TEST_F(JoltSoftBodyCollisionTest, RotatingCandidateIsExplicitlyUnsupportedAndPreservesOutput)
{
    PhysicsBodyDesc body;
    body.kind           = BodyKind::Kinematic;
    body.shape.halfExt  = {2, .25f, .25f};
    const auto rotating = world->CreateBody(body);
    ASSERT_TRUE(IsValid(rotating));
    Transform target;
    target.rotation = Quaternion(1.f, Vector3(0, 0, 1));
    ASSERT_TRUE(world->SetKinematicTarget(rotating, target));
    Step();
    std::vector<SoftBodyCollisionContact> contacts(1);
    contacts.front().point = {9, 8, 7};
    const auto status      = queryProvider->QuerySoftBodyContacts(Query({0, 0, 0}, {0, 0, 0}), contacts);
    EXPECT_EQ(status.error, PhysicsError::Unsupported);
    ASSERT_EQ(contacts.size(), 1u);
    EXPECT_EQ(contacts.front().point, Vector3(9, 8, 7));
    ASSERT_TRUE(queryProvider->QuerySoftBodyContacts(Query({100, 0, 0}, {101, 0, 0}), contacts));
    EXPECT_TRUE(contacts.empty());
}

TEST_F(JoltSoftBodyCollisionTest, DynamicContactsAreMarkedAndQueriesApplyNoReaction)
{
    const auto dynamic = Sphere({0, 0, 0}, BodyKind::Dynamic);
    ASSERT_TRUE(IsValid(dynamic));
    Step();
    Vector3 linearBefore, angularBefore;
    ASSERT_TRUE(world->GetBodyVelocity(dynamic, linearBefore, angularBefore));
    std::vector<SoftBodyCollisionContact> contacts;
    ASSERT_TRUE(queryProvider->QuerySoftBodyContacts(Query({-3, 0, 0}, {3, 0, 0}), contacts));
    ASSERT_FALSE(contacts.empty());
    EXPECT_TRUE(contacts.front().dynamic);
    Vector3 linearAfter, angularAfter;
    ASSERT_TRUE(world->GetBodyVelocity(dynamic, linearAfter, angularAfter));
    EXPECT_EQ(linearBefore, linearAfter);
    EXPECT_EQ(angularBefore, angularAfter);
}
