#include <softbody/solver/CpuSoftBodySolver.h>

#include <gtest/gtest.h>
#include <physics/softbody/ISoftBodyCollisionQuery.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>

using namespace sky;
using namespace sky::phy;
using namespace sky::softbody;

namespace {
    class Collision final : public ISoftBodyCollisionQuery {
    public:
        Vector3 gravity                                                              = VEC3_ZERO;
        enum class Mode { Empty, Invalid, Dynamic, Infeasible, Failure, Floor } mode = Mode::Empty;
        mutable uint64_t calls                                                       = 0;
        uint64_t         failAfter                                                   = UINT64_MAX;
        Vector3          GetGravity() const override
        {
            return gravity;
        }
        PhysicsStatus QuerySoftBodyContacts(const SoftBodyCollisionQuery &query, std::vector<SoftBodyCollisionContact> &out) const override
        {
            if (++calls > failAfter || mode == Mode::Failure)
                return {PhysicsError::BackendFailure, "injected query failure"};
            std::vector<SoftBodyCollisionContact> result;
            if (mode == Mode::Floor) {
                if (std::min(query.start.y, query.end.y) <= query.radius + .00005f)
                    result.push_back({{1, 1}, {query.end.x, 0, query.end.z}, VEC3_Y});
            } else if (mode != Mode::Empty) {
                SoftBodyCollisionContact contact;
                contact.object  = {1, 1};
                contact.normal  = {1, 0, 0};
                contact.point   = {1, 0, 0};
                contact.dynamic = mode == Mode::Dynamic;
                if (mode == Mode::Invalid)
                    contact.normal.x = std::numeric_limits<float>::quiet_NaN();
                result.push_back(contact);
                if (mode == Mode::Infeasible) {
                    contact.object = {2, 1};
                    contact.normal = {-1, 0, 0};
                    contact.point  = VEC3_ZERO;
                    result.push_back(contact);
                }
            }
            out = std::move(result);
            return {};
        }
    };

    SoftBodyDesc Tet()
    {
        SoftBodyDesc desc;
        desc.gravityFactor    = 0;
        desc.linearDamping    = 0;
        desc.vertexRadius     = .01f;
        desc.solverIterations = 4;
        for (const auto point : {Vector3{0, 0, 0}, Vector3{1, 0, 0}, Vector3{0, 1, 0}, Vector3{0, 0, 1}})
            desc.mesh.vertices.push_back({point, VEC3_ZERO, 1.f, SoftBodyAttachmentMode::Dynamic});
        for (uint32_t a = 0; a < 4; ++a) {
            for (uint32_t b = a + 1; b < 4; ++b)
                desc.mesh.edges.push_back({{a, b}, 0});
            desc.rootFollow.push_back({a, .02f});
        }
        desc.mesh.tetrahedra.push_back({{0, 1, 2, 3}, 0});
        desc.mesh.faces = {{1, 2, 3}, {0, 3, 2}, {0, 1, 3}, {0, 2, 1}};
        return desc;
    }
    void Same(const SoftBodySnapshot &a, const SoftBodySnapshot &b)
    {
        EXPECT_EQ(a.id, b.id);
        EXPECT_EQ(a.step, b.step);
        EXPECT_EQ(a.revision, b.revision);
        EXPECT_EQ(a.worldPositions, b.worldPositions);
        EXPECT_EQ(a.worldVelocities, b.worldVelocities);
    }
    double Volume(const SoftBodySnapshot &snapshot)
    {
        const auto &p = snapshot.worldPositions;
        return (p[1] - p[0]).Dot((p[2] - p[0]).Cross(p[3] - p[0])) / 6.0;
    }

    std::array<SoftBodyDesc, 2> ContactPair(float gap)
    {
        const Vector3               normal = Vector3{1, 1, 1} / std::sqrt(3.f);
        std::array<SoftBodyDesc, 2> pair{Tet(), Tet()};
        for (auto &body : pair) {
            body.rootFollow.clear();
            for (auto &edge : body.mesh.edges)
                edge.compliance = 1000.f;
            body.mesh.tetrahedra[0].compliance = 1000.f;
        }
        for (size_t i = 0; i < pair[0].mesh.vertices.size(); ++i) {
            auto &vertex    = pair[0].mesh.vertices[i];
            vertex.position = vertex.position * .2f + Vector3{1.f / 3, 1.f / 3, 1.f / 3} + normal * gap;
            if (i != 0) {
                vertex.mode        = SoftBodyAttachmentMode::WorldFixed;
                vertex.inverseMass = 0;
            }
        }
        for (auto &vertex : pair[1].mesh.vertices)
            vertex.inverseMass = .5f;
        return pair;
    }

    std::array<SoftBodyDesc, 2> MultiContactPair(float velocityScale)
    {
        const std::array<std::array<Vector3, 4>, 2> positions{{{{{.08541176468f, .02584222704f, -.04019431025f},
                                                                 {-.5102498531f, .009999999776f, -.1967634112f},
                                                                 {-.5610343218f, .009999999776f, .1963994503f},
                                                                 {-.1064075455f, .3115221262f, -.1291785687f}}},
                                                               {{{-.08356721699f, .01213727612f, .0404397659f},
                                                                 {.5100918412f, .009999999776f, .1951837689f},
                                                                 {.5619509816f, .009999999776f, -.1946968436f},
                                                                 {.1037738696f, .3055929542f, .1288268268f}}}}};
        const std::array<std::array<Vector3, 4>, 2> velocities{{{{{1.505162716f, .1026607454f, -.5347231627f},
                                                                  {1.460800886f, 0, -.03816752508f},
                                                                  {1.589046955f, .0005718219909f, -.06345911324f},
                                                                  {1.484083414f, .1315885782f, -.04588927701f}}},
                                                                {{{-1.445590854f, -.1876902431f, .5729486942f},
                                                                  {-1.453419328f, 0, .0147850439f},
                                                                  {-1.567734003f, 0, .05974218249f},
                                                                  {-1.566552877f, -.02326373383f, .03498899564f}}}}};
        std::array<SoftBodyDesc, 2>                 pair{Tet(), Tet()};
        for (size_t body = 0; body < pair.size(); ++body) {
            auto &desc = pair[body];
            for (size_t i = 0; i < positions[body].size(); ++i) {
                desc.mesh.vertices[i].position = positions[body][i];
                desc.mesh.vertices[i].velocity = velocities[body][i] * velocityScale;
            }
            for (auto &edge : desc.mesh.edges)
                edge.compliance = .002f;
            desc.mesh.tetrahedra[0].compliance = 2e-16f;
            desc.mesh.faces                    = {{0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3}};
            desc.gravityFactor                 = 1;
            desc.linearDamping                 = 2;
            desc.friction                      = .25f;
        }
        return pair;
    }
} // namespace

TEST(CpuSoftBodySolver, FollowComplianceAndMassUseOneStepWithAccumulatedMultipliers)
{
    constexpr float h = 1.f / 60.f;
    Collision       collision;
    for (const uint32_t iterations : {1u, 4u}) {
        for (const float inverseMass : {1.f, .5f}) {
            auto desc             = Tet();
            desc.solverIterations = iterations;
            for (auto &point : desc.mesh.vertices)
                point.inverseMass = inverseMass;
            CpuSoftBodySolver solver;
            ASSERT_TRUE(solver.Initialize(desc, Uuid::Create()));
            Transform target;
            target.translation.x = 1.f;
            ASSERT_TRUE(solver.SetRootTarget(target));
            ASSERT_TRUE(solver.Step(h, collision, 1));
            const auto  snapshot = solver.GetSnapshot();
            const float expected = inverseMass / (inverseMass + .02f / (h * h));
            for (size_t i = 0; i < snapshot.worldPositions.size(); ++i) {
                EXPECT_NEAR(snapshot.worldPositions[i].x - desc.mesh.vertices[i].position.x, expected, 2e-6f);
                EXPECT_NEAR(snapshot.worldVelocities[i].x, expected / h, 2e-4f);
            }
        }
    }
}

TEST(CpuSoftBodySolver, WorldGravityIntegratesOnceAndPreservesRestShape)
{
    auto desc = Tet();
    desc.rootFollow.clear();
    desc.gravityFactor = 2;
    Collision collision;
    collision.gravity = {0, -9.81f, 0};
    CpuSoftBodySolver solver;
    ASSERT_TRUE(solver.Initialize(desc, Uuid::Create()));
    constexpr float h = 1.f / 60.f;
    ASSERT_TRUE(solver.Step(h, collision, 1));
    const auto snapshot = solver.GetSnapshot();
    for (size_t i = 0; i < snapshot.worldPositions.size(); ++i) {
        EXPECT_NEAR(snapshot.worldPositions[i].y - desc.mesh.vertices[i].position.y, -19.62f * h * h, 1e-6f);
        EXPECT_NEAR(snapshot.worldVelocities[i].y, -19.62f * h, 1e-5f);
    }
    EXPECT_NEAR(Volume(snapshot), 1.0 / 6.0, 1e-6);
}

TEST(CpuSoftBodySolver, LiveParametersPreserveMotionAndAffectTheNextStep)
{
    auto desc                     = Tet();
    desc.mesh.edges[0].compliance = .003f;
    CpuSoftBodySolver solver;
    ASSERT_TRUE(solver.Initialize(desc, Uuid::Create()));
    Collision collision;
    Transform root;
    root.translation.x = .5f;
    ASSERT_TRUE(solver.SetRootTarget(root));
    constexpr float h = 1.f / 60.f;
    ASSERT_TRUE(solver.Step(h, collision, 10));
    const auto before = solver.GetSnapshot();
    ASSERT_GT(before.worldVelocities[0].x, 0.f);
    SoftBodyRuntimeParameters read;
    ASSERT_TRUE(solver.ReadParameters(read));
    EXPECT_EQ(read.edgeCompliance.minimum, 0.f);
    EXPECT_EQ(read.edgeCompliance.maximum, .003f);

    std::array<CpuSoftBodySolver *, 1> selected{&solver};
    SoftBodyParameterPatch             patch;
    patch.followCompliance = .04f;
    patch.linearDamping    = 3.f;
    ASSERT_TRUE(CpuSoftBodySolver::ApplyParameters(selected, patch));
    Same(before, solver.GetSnapshot());
    ASSERT_TRUE(solver.ReadParameters(read));
    EXPECT_EQ(read.edgeCompliance.maximum, .003f);
    EXPECT_EQ(read.followCompliance.minimum, .04f);
    EXPECT_EQ(read.followCompliance.maximum, .04f);
    ASSERT_TRUE(solver.Step(h, collision, 11));
    const float predicted = before.worldPositions[0].x + h * before.worldVelocities[0].x;
    const float expected  = predicted + (.5f - predicted) / (1.f + .04f / (h * h));
    const auto  after     = solver.GetSnapshot();
    EXPECT_NEAR(after.worldPositions[0].x, expected, 1e-6f);
    EXPECT_NEAR(after.worldVelocities[0].x, (expected - before.worldPositions[0].x) / h * std::exp(-3.f * h), 1e-5f);
}

TEST(CpuSoftBodySolver, VolumeComplianceChangesCompressionResistance)
{
    Collision collision;
    double    errors[2]{};
    for (size_t index = 0; index < 2; ++index) {
        auto desc = Tet();
        desc.rootFollow.clear();
        desc.solverIterations = 8;
        for (auto &edge : desc.mesh.edges)
            edge.compliance = 1000;
        desc.mesh.tetrahedra[0].compliance = index == 0 ? 0.f : 1000.f;
        desc.mesh.vertices[3].velocity.z   = -1;
        CpuSoftBodySolver solver;
        ASSERT_TRUE(solver.Initialize(desc, Uuid::Create()));
        ASSERT_TRUE(solver.Step(1.f / 60.f, collision, 1));
        const auto volume = Volume(solver.GetSnapshot());
        EXPECT_GT(volume, 0);
        errors[index] = std::abs(volume - 1.0 / 6.0);
    }
    EXPECT_LT(errors[0], 1e-6);
    EXPECT_GT(errors[1], .001);
}

TEST(CpuSoftBodySolver, OptionalFixedAndRootKinematicPointsKeepTheirDistinctTargets)
{
    auto desc                         = Tet();
    desc.mesh.vertices[0].mode        = SoftBodyAttachmentMode::WorldFixed;
    desc.mesh.vertices[1].mode        = SoftBodyAttachmentMode::RootKinematic;
    desc.mesh.vertices[0].inverseMass = desc.mesh.vertices[1].inverseMass = 0;
    desc.rootFollow.erase(desc.rootFollow.begin(), desc.rootFollow.begin() + 2);
    Collision         collision;
    CpuSoftBodySolver solver;
    ASSERT_TRUE(solver.Initialize(desc, Uuid::Create()));
    Transform target;
    target.translation.z = .03f;
    ASSERT_TRUE(solver.SetRootTarget(target));
    ASSERT_TRUE(solver.Step(1.f / 60.f, collision, 1));
    const auto snapshot = solver.GetSnapshot();
    EXPECT_EQ(snapshot.worldPositions[0], desc.mesh.vertices[0].position);
    EXPECT_EQ(snapshot.worldPositions[1], target.Translate(desc.mesh.vertices[1].position));
    EXPECT_NEAR(snapshot.worldVelocities[1].z, 1.8f, 1e-5f);
}

TEST(CpuSoftBodySolver, InvalidOrUnsupportedContactsPreserveTheCompletePreviousSnapshot)
{
    Collision         collision;
    CpuSoftBodySolver solver;
    ASSERT_TRUE(solver.Initialize(Tet(), Uuid::Create()));
    ASSERT_TRUE(solver.Step(1.f / 60.f, collision, 1));
    const auto before = solver.GetSnapshot();
    for (const auto mode : {Collision::Mode::Invalid, Collision::Mode::Dynamic, Collision::Mode::Infeasible, Collision::Mode::Failure}) {
        collision.mode = mode;
        EXPECT_FALSE(solver.Step(1.f / 60.f, collision, 2));
        Same(solver.GetSnapshot(), before);
    }
    collision.mode = Collision::Mode::Empty;
    ASSERT_TRUE(solver.Step(1.f / 60.f, collision, 3));
    EXPECT_EQ(solver.GetSnapshot().step, 3u);
}

TEST(CpuSoftBodySolver, OverflowingInitializationAndResetPreserveValidState)
{
    CpuSoftBodySolver solver;
    auto              desc = Tet();
    ASSERT_TRUE(solver.Initialize(desc, Uuid::Create()));
    const auto before = solver.GetSnapshot();
    Transform  huge;
    huge.translation = {1e30f, 1e30f, 1e30f};
    desc.transform   = huge;
    EXPECT_FALSE(solver.Initialize(desc, Uuid::Create()));
    Same(solver.GetSnapshot(), before);
    EXPECT_FALSE(solver.Reset(huge));
    Same(solver.GetSnapshot(), before);
    desc          = Tet();
    desc.pressure = 1;
    EXPECT_EQ(solver.Initialize(desc, Uuid::Create()).error, PhysicsError::Unsupported);
    Same(solver.GetSnapshot(), before);
    Transform reset;
    reset.translation.x = 2;
    ASSERT_TRUE(solver.Reset(reset));
    EXPECT_EQ(solver.GetSnapshot().id, before.id);
    EXPECT_EQ(solver.GetSnapshot().revision, before.revision + 1);
    for (const auto &velocity : solver.GetSnapshot().worldVelocities)
        EXPECT_EQ(velocity, VEC3_ZERO);
}

TEST(CpuSoftBodySolver, IdenticalFixedStepInputsAgreeAcrossDisplayRates)
{
    const auto run = [](uint32_t fps) {
        CpuSoftBodySolver solver;
        Collision         collision;
        EXPECT_TRUE(solver.Initialize(Tet(), Uuid::Create()));
        constexpr double fixed       = 1.0 / 60.0;
        double           accumulator = 0;
        uint64_t         step        = 0;
        for (uint32_t frame = 0; frame < fps * 2; ++frame) {
            accumulator += 1.0 / fps;
            while (accumulator + 1e-12 >= fixed) {
                Transform target;
                target.translation.x = .1f * std::sin(static_cast<float>(++step * fixed));
                EXPECT_TRUE(solver.SetRootTarget(target));
                EXPECT_TRUE(solver.Step(static_cast<float>(fixed), collision, step));
                accumulator -= fixed;
            }
        }
        return solver.GetSnapshot();
    };
    const auto baseline = run(60);
    EXPECT_EQ(baseline.step, 120u);
    for (const auto fps : {30u, 120u}) {
        const auto actual = run(fps);
        EXPECT_EQ(actual.step, baseline.step);
        EXPECT_EQ(actual.worldPositions, baseline.worldPositions);
        EXPECT_EQ(actual.worldVelocities, baseline.worldVelocities);
    }
}

TEST(CpuSoftBodySolver, BatchContactUsesTriangleBarycentricsAndBothInverseMasses)
{
    const auto                       descriptions = ContactPair(.01f);
    std::array<CpuSoftBodySolver, 2> solvers;
    for (size_t i = 0; i < solvers.size(); ++i)
        ASSERT_TRUE(solvers[i].Initialize(descriptions[i], Uuid::Create()));
    const std::array<CpuSoftBodySolver *, 2> bodies{&solvers[0], &solvers[1]};
    Collision                                collision;
    SoftBodySimulationSettings               settings;
    settings.softCollision  = true;
    settings.rigidCollision = false;
    SoftBodyStepMetrics metrics;
    const auto          status = CpuSoftBodySolver::StepBatch(bodies, 1.f / 60, collision, 1, settings, &metrics);
    ASSERT_TRUE(status) << status.message;
    const Vector3 normal = Vector3{1, 1, 1} / std::sqrt(3.f);
    const auto    a = solvers[0].GetSnapshot(), b = solvers[1].GetSnapshot();
    const float   multiplier = .01f / (1.f + .5f / 3.f);
    EXPECT_NEAR((a.worldPositions[0] - descriptions[0].mesh.vertices[0].position).Dot(normal), multiplier, 2e-6f);
    Vector3 momentum;
    momentum += a.worldVelocities[0];
    for (size_t i = 0; i < 4; ++i) {
        momentum += b.worldVelocities[i] * 2.f;
        if (i != 0)
            EXPECT_NEAR((b.worldPositions[i] - descriptions[1].mesh.vertices[i].position).Dot(normal), -multiplier / 6.f, 2e-6f);
    }
    EXPECT_LT(momentum.Length(), 2e-5f);
    EXPECT_TRUE(metrics.committed);
    EXPECT_EQ(metrics.completedSteps, 1u);
    EXPECT_EQ(metrics.bodyCount, 2u);
    EXPECT_EQ(metrics.internalIterations, 8u);
    EXPECT_GT(metrics.softContacts, 0u);
    EXPECT_GT(metrics.activeSoftContacts, 0u);
    EXPECT_GT(metrics.vertexTriangleTests, 0u);
    EXPECT_GT(metrics.tetContainmentTests, 0u);
    EXPECT_EQ(metrics.rigidQueries, 0u);
    EXPECT_EQ(metrics.totalNs, 0u);
    EXPECT_LE(metrics.maxPenetration, .00005);
}

TEST(CpuSoftBodySolver, SmallCoupledFloorAndShellResidualIsResolvedWithoutMoreInternalIterations)
{
    const std::array<float, 3>         speeds{.25f, 1.f, 1.7f};
    std::array<SoftBodyStepMetrics, 3> metrics;
    for (size_t variant = 0; variant < metrics.size(); ++variant) {
        const auto                       descriptions = MultiContactPair(speeds[variant]);
        std::array<CpuSoftBodySolver, 2> solvers;
        for (size_t i = 0; i < solvers.size(); ++i)
            ASSERT_TRUE(solvers[i].Initialize(descriptions[i], Uuid::Create()));
        const std::array<CpuSoftBodySolver *, 2> bodies{&solvers[0], &solvers[1]};
        Collision                                collision;
        collision.mode    = Collision::Mode::Floor;
        collision.gravity = {0, -9.8f, 0};
        SoftBodySimulationSettings settings;
        settings.softCollision = settings.rigidCollision = settings.profileSteps = true;
        // The original four rounds left 0.127804 mm and 0.368349 mm in the two faster cases.
        const auto status = CpuSoftBodySolver::StepBatch(bodies, 1.f / 60.f, collision, 1, settings, &metrics[variant]);
        ASSERT_TRUE(status) << status.message << " residual=" << metrics[variant].maxPenetration;
        EXPECT_TRUE(metrics[variant].committed);
        EXPECT_EQ(metrics[variant].internalIterations, 8u);
        EXPECT_LE(metrics[variant].maxPenetration, .00005);
        for (auto &solver : solvers) {
            const auto snapshot = solver.GetSnapshot();
            EXPECT_EQ(snapshot.step, 1u);
            EXPECT_GT(Volume(snapshot), 0.);
            for (const auto &position : snapshot.worldPositions)
                EXPECT_GE(position.y, .01f - .00005f);
        }
    }
    for (size_t variant = 1; variant < metrics.size(); ++variant) {
        EXPECT_GT(metrics[variant].vertexTriangleTests, metrics[0].vertexTriangleTests);
        EXPECT_GT(metrics[variant].rigidQueries, metrics[0].rigidQueries);
        EXPECT_GT(metrics[variant].softProjectionNs, 0u);
    }
}

TEST(CpuSoftBodySolver, LargeCoupledContactResidualStillRejectsTheWholeBatch)
{
    const auto                  small = MultiContactPair(1.f), large = MultiContactPair(2.f);
    std::array<SoftBodyDesc, 4> descriptions{small[0], small[1], large[0], large[1]};
    for (size_t i = 2; i < descriptions.size(); ++i)
        for (auto &vertex : descriptions[i].mesh.vertices)
            vertex.position.z += 3.f;
    std::array<CpuSoftBodySolver, 4> solvers;
    std::array<SoftBodySnapshot, 4>  previous;
    for (size_t i = 0; i < solvers.size(); ++i)
        ASSERT_TRUE(solvers[i].Initialize(descriptions[i], Uuid::Create()));
    for (size_t i = 0; i < solvers.size(); ++i)
        previous[i] = solvers[i].GetSnapshot();
    const std::array<CpuSoftBodySolver *, 4> bodies{&solvers[0], &solvers[1], &solvers[2], &solvers[3]};
    Collision                                collision;
    collision.mode    = Collision::Mode::Floor;
    collision.gravity = {0, -9.8f, 0};
    SoftBodySimulationSettings settings;
    settings.softCollision = settings.rigidCollision = true;
    SoftBodyStepMetrics metrics;
    const auto          status = CpuSoftBodySolver::StepBatch(bodies, 1.f / 60.f, collision, 1, settings, &metrics);
    EXPECT_FALSE(status);
    EXPECT_GT(metrics.maxPenetration, .0004);
    EXPECT_NE(status.message.find("step=1"), std::string::npos);
    EXPECT_NE(status.message.find("reason=shell_limit"), std::string::npos);
    EXPECT_NE(status.message.find("qualification_limit_m=0.0004"), std::string::npos);
    EXPECT_NE(status.message.find("center_inside=0"), std::string::npos);
    EXPECT_FALSE(metrics.committed);
    EXPECT_EQ(metrics.completedSteps, 0u);
    for (size_t i = 0; i < solvers.size(); ++i)
        Same(solvers[i].GetSnapshot(), previous[i]);
}

TEST(CpuSoftBodySolver, ResidualFinalRigidQueryFailurePreservesEverySnapshot)
{
    const auto                       descriptions = MultiContactPair(1.f);
    std::array<CpuSoftBodySolver, 2> solvers;
    for (size_t i = 0; i < solvers.size(); ++i)
        ASSERT_TRUE(solvers[i].Initialize(descriptions[i], Uuid::Create()));
    const auto                               previousA = solvers[0].GetSnapshot(), previousB = solvers[1].GetSnapshot();
    const std::array<CpuSoftBodySolver *, 2> bodies{&solvers[0], &solvers[1]};
    Collision                                collision;
    collision.mode    = Collision::Mode::Floor;
    collision.gravity = {0, -9.8f, 0};
    // Four ordinary rounds, initial validation and a repair round precede final revalidation.
    collision.failAfter = (descriptions[0].solverIterations + 2) * descriptions[0].mesh.vertices.size() * solvers.size();
    SoftBodySimulationSettings settings;
    settings.softCollision = settings.rigidCollision = true;
    SoftBodyStepMetrics metrics;
    const auto          status = CpuSoftBodySolver::StepBatch(bodies, 1.f / 60.f, collision, 1, settings, &metrics);
    EXPECT_EQ(status.error, PhysicsError::BackendFailure);
    EXPECT_FALSE(metrics.committed);
    EXPECT_EQ(metrics.internalIterations, 8u);
    Same(solvers[0].GetSnapshot(), previousA);
    Same(solvers[1].GetSnapshot(), previousB);
}

TEST(CpuSoftBodySolver, BatchFindsDeepContainedVerticesBeyondTheContactRadius)
{
    const auto                       descriptions = ContactPair(-.04f);
    std::array<CpuSoftBodySolver, 2> solvers;
    for (size_t i = 0; i < solvers.size(); ++i)
        ASSERT_TRUE(solvers[i].Initialize(descriptions[i], Uuid::Create()));
    const std::array<CpuSoftBodySolver *, 2> bodies{&solvers[0], &solvers[1]};
    Collision                                collision;
    SoftBodySimulationSettings               settings;
    settings.softCollision  = true;
    settings.rigidCollision = false;
    settings.profileSteps   = true;
    SoftBodyStepMetrics metrics;
    const auto          status = CpuSoftBodySolver::StepBatch(bodies, 1.f / 60, collision, 1, settings, &metrics);
    ASSERT_TRUE(status) << status.message;
    const auto    a = solvers[0].GetSnapshot(), b = solvers[1].GetSnapshot();
    const Vector3 normal = Vector3{1, 1, 1} / std::sqrt(3.f);
    EXPECT_GT((a.worldPositions[0] - descriptions[0].mesh.vertices[0].position).Dot(normal), .04f);
    const Vector3 face = (b.worldPositions[1] + b.worldPositions[2] + b.worldPositions[3]) / 3.f;
    EXPECT_NEAR((a.worldPositions[0] - face).Dot(normal), .02f, 2e-5f);
    EXPECT_GT(metrics.totalNs, 0u);
    EXPECT_GT(metrics.softNarrowphaseNs, 0u);
}

TEST(CpuSoftBodySolver, BatchFrictionReducesRelativeTangentialMotionWithoutLosingMomentum)
{
    std::array<float, 2> tangentialSpeeds{};
    for (size_t variant = 0; variant < 2; ++variant) {
        auto descriptions                         = ContactPair(.01f);
        descriptions[0].mesh.vertices[0].velocity = {.01f, -.01f, 0};
        for (auto &body : descriptions)
            body.friction = variant == 0 ? 0.f : .5f;
        std::array<CpuSoftBodySolver, 2> solvers;
        for (size_t i = 0; i < solvers.size(); ++i)
            ASSERT_TRUE(solvers[i].Initialize(descriptions[i], Uuid::Create()));
        const std::array<CpuSoftBodySolver *, 2> bodies{&solvers[0], &solvers[1]};
        Collision                                collision;
        SoftBodySimulationSettings               settings;
        settings.softCollision  = true;
        settings.rigidCollision = false;
        const auto status       = CpuSoftBodySolver::StepBatch(bodies, 1.f / 60, collision, 1, settings);
        ASSERT_TRUE(status) << status.message;
        const auto    a = solvers[0].GetSnapshot(), b = solvers[1].GetSnapshot();
        const Vector3 faceVelocity = (b.worldVelocities[1] + b.worldVelocities[2] + b.worldVelocities[3]) / 3.f;
        const Vector3 tangent      = Vector3{1, -1, 0} / std::sqrt(2.f);
        tangentialSpeeds[variant]  = std::abs((a.worldVelocities[0] - faceVelocity).Dot(tangent));
        Vector3 momentum           = a.worldVelocities[0];
        for (const auto &velocity : b.worldVelocities)
            momentum += velocity * 2.f;
        EXPECT_LT((momentum - Vector3{.01f, -.01f, 0}).Length(), 2e-5f);
    }
    EXPECT_GT(tangentialSpeeds[0], .01f);
    EXPECT_LT(tangentialSpeeds[1], tangentialSpeeds[0] * .01f);
}

TEST(CpuSoftBodySolver, BatchRejectsEdgeFaceIntersectionsWithoutContainedVertices)
{
    auto                         first = Tet(), second = Tet();
    const std::array<Vector3, 4> points{Vector3{1, 1, 1}, {-1, -1, 1}, {-1, 1, -1}, {1, -1, -1}};
    first.rootFollow.clear();
    second.rootFollow.clear();
    for (size_t i = 0; i < 4; ++i) {
        first.mesh.vertices[i].position  = points[i];
        second.mesh.vertices[i].position = points[i == 1 ? 2 : i == 2 ? 1 : i] * -1.f;
    }
    std::array<CpuSoftBodySolver, 2> solvers;
    ASSERT_TRUE(solvers[0].Initialize(first, Uuid::Create()));
    ASSERT_TRUE(solvers[1].Initialize(second, Uuid::Create()));
    const auto                               previousA = solvers[0].GetSnapshot(), previousB = solvers[1].GetSnapshot();
    const std::array<CpuSoftBodySolver *, 2> bodies{&solvers[0], &solvers[1]};
    Collision                                collision;
    SoftBodySimulationSettings               settings;
    settings.softCollision  = true;
    settings.rigidCollision = false;
    SoftBodyStepMetrics metrics;
    const auto          status = CpuSoftBodySolver::StepBatch(bodies, 1.f / 60, collision, 1, settings, &metrics);
    EXPECT_EQ(status.error, PhysicsError::Unsupported) << status.message;
    EXPECT_GT(metrics.segmentTriangleTests, 0u);
    EXPECT_FALSE(metrics.committed);
    EXPECT_EQ(metrics.completedSteps, 0u);
    Same(solvers[0].GetSnapshot(), previousA);
    Same(solvers[1].GetSnapshot(), previousB);
}

TEST(CpuSoftBodySolver, BatchValidationAndProviderFailurePreserveEverySnapshot)
{
    std::array<CpuSoftBodySolver, 2> solvers;
    const auto                       id = Uuid::Create();
    ASSERT_TRUE(solvers[0].Initialize(Tet(), id));
    ASSERT_TRUE(solvers[1].Initialize(Tet(), id));
    const auto                         initial = solvers[0].GetSnapshot();
    std::array<CpuSoftBodySolver *, 2> bodies{&solvers[0], &solvers[1]};
    Collision                          collision;
    SoftBodyStepMetrics                metrics;
    SoftBodySimulationSettings         settings;
    EXPECT_FALSE(CpuSoftBodySolver::StepBatch(bodies, 1.f / 60, collision, 1, settings, &metrics));
    EXPECT_EQ(metrics.failure, PhysicsError::InvalidArgument);
    bodies[1] = bodies[0];
    EXPECT_FALSE(CpuSoftBodySolver::StepBatch(bodies, 1.f / 60, collision, 1, settings));
    bodies[1] = &solvers[1];
    ASSERT_TRUE(solvers[1].Initialize(Tet(), Uuid::Create()));
    const auto second              = solvers[1].GetSnapshot();
    settings.softContactCompliance = -1;
    EXPECT_FALSE(CpuSoftBodySolver::StepBatch(bodies, 1.f / 60, collision, 1, settings));
    settings.softContactCompliance = 0;
    EXPECT_FALSE(CpuSoftBodySolver::StepBatch(bodies, 1.f / 60, collision, 0, settings));
    collision.failAfter = 36;
    EXPECT_FALSE(CpuSoftBodySolver::StepBatch(bodies, 1.f / 60, collision, 1, settings, &metrics));
    EXPECT_EQ(metrics.failure, PhysicsError::BackendFailure);
    EXPECT_EQ(metrics.rigidQueries, 37u);
    EXPECT_EQ(metrics.completedSteps, 0u);
    Same(solvers[0].GetSnapshot(), initial);
    Same(solvers[1].GetSnapshot(), second);
    collision.failAfter = UINT64_MAX;
    ASSERT_TRUE(CpuSoftBodySolver::StepBatch(bodies, 1.f / 60, collision, 2, settings, &metrics));
    EXPECT_EQ(metrics.rigidQueries, 40u);
    EXPECT_EQ(metrics.internalIterations, 8u);
    EXPECT_EQ(metrics.completedSteps, 1u);
}

TEST(CpuSoftBodySolver, SeparateBatchesNeverIncludeUnregisteredBodies)
{
    std::array<CpuSoftBodySolver, 2> solvers;
    const auto                       id = Uuid::Create();
    for (auto &solver : solvers)
        ASSERT_TRUE(solver.Initialize(Tet(), id));
    const auto                               untouched = solvers[1].GetSnapshot();
    const std::array<CpuSoftBodySolver *, 1> first{&solvers[0]}, second{&solvers[1]};
    Collision                                collision;
    SoftBodySimulationSettings               settings;
    settings.softCollision  = true;
    settings.rigidCollision = false;
    SoftBodyStepMetrics metrics;
    ASSERT_TRUE(CpuSoftBodySolver::StepBatch(first, 1.f / 60, collision, 1, settings, &metrics));
    EXPECT_EQ(metrics.bodyCount, 1u);
    EXPECT_EQ(metrics.broadphasePairs, 0u);
    Same(solvers[1].GetSnapshot(), untouched);
    ASSERT_TRUE(CpuSoftBodySolver::StepBatch(second, 1.f / 60, collision, 1, settings, &metrics));
    EXPECT_EQ(metrics.softContacts, 0u);
}
