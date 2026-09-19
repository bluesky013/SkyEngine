//
// LodGroup tests: level access, bounds, and screen-size driven LOD selection.
//

#include <aurora/resource/LodGroup.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

namespace {

    CounterPtr<Mesh> MakeMesh()
    {
        return CounterPtr<Mesh>(new Mesh());
    }

    void AddLevels(LodGroup &group)
    {
        group.AddLevel(LodLevel{1.0f, MakeMesh()});
        group.AddLevel(LodLevel{0.5f, MakeMesh()});
        group.AddLevel(LodLevel{0.25f, MakeMesh()});
    }

} // namespace

TEST(LodGroupTest, SelectLodThreshold)
{
    LodGroup group;
    AddLevels(group);

    EXPECT_EQ(group.SelectLod(0.75f), 0u);
    EXPECT_EQ(group.SelectLod(0.4f), 1u);
    EXPECT_EQ(group.SelectLod(0.1f), 2u);
}

TEST(LodGroupTest, GetMeshOutOfRange)
{
    LodGroup group;
    AddLevels(group);

    EXPECT_EQ(group.GetLevelCount(), 3u);
    EXPECT_NE(group.GetMesh(0), nullptr);
    EXPECT_EQ(group.GetMesh(3), nullptr);
    EXPECT_EQ(group.GetMesh(100), nullptr);
}

TEST(LodGroupTest, NullMeshLevelResolvesNull)
{
    LodGroup group;
    group.AddLevel(LodLevel{1.f, {}});

    EXPECT_EQ(group.GetMesh(0), nullptr);
    EXPECT_EQ(group.SelectLod(1.f), 0u);
}

TEST(LodGroupTest, EmptyGroupBoundingSphere)
{
    const LodGroup group;

    EXPECT_EQ(group.GetLevelCount(), 0u);
    EXPECT_FLOAT_EQ(group.GetBoundingSphere().radius, 0.f);
}

TEST(LodGroupTest, ProjectionOverloadMatchesScreenSize)
{
    LodGroup group;
    AddLevels(group);

    BoundingBoxSphere bounds;
    bounds.center = Vector3(0.f, 0.f, 0.f);
    bounds.radius = 2.f;

    const Vector3 viewOrigin(0.f, 0.f, -10.f);
    const Matrix4 proj = Matrix4::Identity();

    const float screenSize = LodGroup::CalculateScreenSize(bounds, viewOrigin, proj);
    EXPECT_EQ(group.SelectLod(bounds, viewOrigin, proj), group.SelectLod(screenSize));
}
