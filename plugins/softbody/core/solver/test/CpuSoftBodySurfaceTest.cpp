#include <softbody/solver/CpuSoftBodySolver.h>

#include <gtest/gtest.h>
#include <physics/softbody/ISoftBodyCollisionQuery.h>

#include <algorithm>

using namespace sky;
using namespace sky::phy;
using namespace sky::softbody;

namespace {
    class InternalDynamicObstacle final : public ISoftBodyCollisionQuery {
    public:
        Vector3 center{.25f, .25f, .25f};
        Vector3 GetGravity() const override
        {
            return VEC3_ZERO;
        }
        PhysicsStatus QuerySoftBodyContacts(const SoftBodyCollisionQuery &query, std::vector<SoftBodyCollisionContact> &out) const override
        {
            const auto      direction      = query.end - query.start;
            const float     squared        = direction.Dot(direction);
            const float     fraction       = squared > 0 ? std::clamp((center - query.start).Dot(direction) / squared, 0.f, 1.f) : 0.f;
            const auto      offset         = query.start + direction * fraction - center;
            constexpr float obstacleRadius = .025f;
            const float     radius         = obstacleRadius + query.radius;
            std::vector<SoftBodyCollisionContact> contacts;
            if (offset.Dot(offset) <= radius * radius) {
                const auto normal = offset.Length() > 1e-8f ? offset / offset.Length() : VEC3_X;
                contacts.push_back({{17, 9}, center + normal * obstacleRadius, normal, VEC3_ZERO, fraction, 0, true});
            }
            out = std::move(contacts);
            return {};
        }
    };

    SoftBodyDesc CenteredTetrahedron()
    {
        SoftBodyDesc desc;
        desc.gravityFactor    = 0;
        desc.linearDamping    = 0;
        desc.solverIterations = 4;
        for (const auto point : {Vector3{0, 0, 0}, Vector3{1, 0, 0}, Vector3{0, 1, 0}, Vector3{0, 0, 1}, Vector3{.25f, .25f, .25f}})
            desc.mesh.vertices.push_back({point, VEC3_ZERO, 1.f, SoftBodyAttachmentMode::Dynamic});
        desc.mesh.faces = {{1, 2, 3}, {0, 3, 2}, {0, 1, 3}, {0, 2, 1}};
        for (const auto &face : desc.mesh.faces)
            desc.mesh.tetrahedra.push_back({{4, face[0], face[1], face[2]}, 0});
        for (uint32_t a = 0; a < desc.mesh.vertices.size(); ++a) {
            for (uint32_t b = a + 1; b < desc.mesh.vertices.size(); ++b)
                desc.mesh.edges.push_back({{a, b}, 0});
            desc.rootFollow.push_back({a, .02f});
        }
        return desc;
    }
} // namespace

TEST(CpuSoftBodySurface, InternalDynamicObstacleDoesNotCollideWithTheVolumeCenter)
{
    const auto        desc = CenteredTetrahedron();
    CpuSoftBodySolver solver;
    ASSERT_TRUE(solver.Initialize(desc, Uuid::Create()));
    Transform root;
    root.translation.x = .1f;
    ASSERT_TRUE(solver.SetRootTarget(root));
    InternalDynamicObstacle collision;
    constexpr float         h      = 1.f / 60.f;
    const auto              status = solver.Step(h, collision, 1);
    ASSERT_TRUE(status) << status.message;
    const auto snapshot = solver.GetSnapshot();
    ASSERT_EQ(snapshot.worldPositions.size(), 5u);
    const float expectedShift = root.translation.x / (1.f + .02f / (h * h));
    EXPECT_NEAR(snapshot.worldPositions[4].x, .25f + expectedShift, 2e-6f);
    EXPECT_NEAR(snapshot.worldVelocities[4].x, expectedShift / h, 2e-4f);
    EXPECT_EQ(snapshot.step, 1u);

    collision.center = snapshot.worldPositions[0];
    EXPECT_EQ(solver.Step(h, collision, 2).error, PhysicsError::Unsupported);
    const auto preserved = solver.GetSnapshot();
    EXPECT_EQ(preserved.id, snapshot.id);
    EXPECT_EQ(preserved.step, snapshot.step);
    EXPECT_EQ(preserved.revision, snapshot.revision);
    EXPECT_EQ(preserved.worldPositions, snapshot.worldPositions);
    EXPECT_EQ(preserved.worldVelocities, snapshot.worldVelocities);
}
