#include <gtest/gtest.h>
#include <softbody/world/SoftBodyPose.h>

using namespace sky;
using namespace sky::softbody;

namespace {
    SoftBodyAssetData Product()
    {
        SoftBodyAssetData value;
        value.mesh                 = Uuid::Create();
        value.skin                 = Uuid::Create();
        value.skeleton             = Uuid::Create();
        value.skeletonLayout       = 7;
        value.controlIds           = {"a", "b", "c", "d"};
        value.body.mesh.vertices   = {{{0, 0, 0}}, {{1, 0, 0}}, {{0, 1, 0}}, {{0, 0, 1}}};
        value.body.mesh.edges      = {{{0, 1}}, {{0, 2}}, {{0, 3}}, {{1, 2}}, {{1, 3}}, {{2, 3}}};
        value.body.mesh.tetrahedra = {{{0, 1, 2, 3}}};
        value.body.mesh.faces      = {{1, 2, 3}, {0, 3, 2}, {0, 1, 3}, {0, 2, 1}};
        return value;
    }
    std::vector<Matrix4> Base(const SoftBodyAssetData &data)
    {
        std::vector<Matrix4> base{Matrix4::Identity()};
        for (const auto &v : data.body.mesh.vertices) {
            auto matrix = Matrix4::Identity();
            matrix[3]   = Vector4(v.position.x, v.position.y, v.position.z, 1);
            base.push_back(matrix);
        }
        return base;
    }
} // namespace

TEST(SoftBodyPose, AffineShearScaleAndRigidRootProduceExactControlAndNormalFields)
{
    const auto    data = Product();
    const auto    base = Base(data);
    const Matrix3 deformation({2, .1f, 0}, {.6f, 1.2f, .2f}, {0, .4f, .7f});
    Transform     root;
    root.rotation    = Quaternion(.7f, {0, 1, 0});
    root.translation = {3, -2, 5};
    const Vector3        shift(-.3f, .4f, 2);
    std::vector<Vector3> points;
    for (const auto &v : data.body.mesh.vertices)
        points.push_back(root.Translate(deformation * v.position + shift));
    std::vector<Matrix4> models, normals;
    const auto           status = BuildSoftBodyPose(data, points, root, base, models, normals);
    ASSERT_TRUE(status) << status.message;
    auto affine = Matrix4::Identity();
    for (uint32_t col = 0; col < 3; ++col)
        for (uint32_t row = 0; row < 3; ++row)
            affine[col][row] = deformation[col][row];
    const auto expectedNormal = affine.InverseTranspose();
    ASSERT_EQ(models.size(), 5u);
    ASSERT_EQ(normals.size(), 5u);
    for (size_t i = 0; i < 4; ++i) {
        const auto expected = deformation * data.body.mesh.vertices[i].position + shift;
        for (uint32_t axis = 0; axis < 3; ++axis)
            EXPECT_NEAR(models[i + 1][3][axis], expected[axis], 1e-5f);
        for (uint32_t col = 0; col < 3; ++col)
            for (uint32_t row = 0; row < 3; ++row)
                EXPECT_NEAR(normals[i + 1][col][row], expectedNormal[col][row], 1e-5f);
    }
    for (uint32_t i = 0; i < 16; ++i)
        EXPECT_FLOAT_EQ(models[0].v[i], base[0].v[i]);
}

TEST(SoftBodyPose, StandardSkinningPublishesTranslationsWithoutOverridingSkinNormals)
{
    auto data                 = Product();
    data.normalPolicy         = SoftBodyNormalPolicy::StandardSkinning;
    const auto           base = Base(data);
    std::vector<Vector3> points;
    for (const auto &v : data.body.mesh.vertices)
        points.push_back({1.2f * v.position.x + .1f, .8f * v.position.y, v.position.z + .2f});
    std::vector<Matrix4> models, normals{Matrix4::Identity()};
    const auto           status = BuildSoftBodyPose(data, points, {}, base, models, normals);
    ASSERT_TRUE(status) << status.message;
    EXPECT_TRUE(normals.empty());
    ASSERT_EQ(models.size(), base.size());
    for (size_t i = 0; i < points.size(); ++i)
        for (uint32_t axis = 0; axis < 3; ++axis)
            EXPECT_NEAR(models[i + 1][3][axis], points[i][axis], 1e-6f);
    const auto saved = models;
    points[3].z      = -.2f;
    EXPECT_FALSE(BuildSoftBodyPose(data, points, {}, base, models, normals));
    EXPECT_TRUE(normals.empty());
    ASSERT_EQ(models.size(), saved.size());
    for (size_t i = 0; i < models.size(); ++i)
        for (uint32_t k = 0; k < 16; ++k)
            EXPECT_EQ(models[i].v[k], saved[i].v[k]);
}

TEST(SoftBodyPose, SingularOrInvertedFieldsPreserveCompletePreviousPose)
{
    const auto           data = Product();
    const auto           base = Base(data);
    std::vector<Vector3> points;
    for (const auto &v : data.body.mesh.vertices)
        points.push_back(v.position);
    std::vector<Matrix4> models{Matrix4::Identity()}, normals{Matrix4::Identity()};
    points[3].z = 0;
    EXPECT_FALSE(BuildSoftBodyPose(data, points, {}, base, models, normals));
    EXPECT_EQ(models.size(), 1u);
    EXPECT_EQ(normals.size(), 1u);
    points[3].z = -1;
    EXPECT_FALSE(BuildSoftBodyPose(data, points, {}, base, models, normals));
    EXPECT_EQ(models.size(), 1u);
    EXPECT_EQ(normals.size(), 1u);
    points[3].z = 1;
    Transform nonrigid;
    nonrigid.scale = {2, 1, 1};
    EXPECT_FALSE(BuildSoftBodyPose(data, points, nonrigid, base, models, normals));
    EXPECT_EQ(models.size(), 1u);
    EXPECT_EQ(normals.size(), 1u);
}

TEST(SoftBodyPose, FiniteBaseWithOverflowingDeterminantPreservesOutput)
{
    const auto           data = Product();
    auto                 base = Base(data);
    std::vector<Vector3> points;
    for (const auto &vertex : data.body.mesh.vertices)
        points.push_back(vertex.position);
    base[0][0].x = 1e30f;
    base[0][1].y = 1e30f;
    base[0][2].z = 1e30f;
    std::vector<Matrix4> models{Matrix4::Identity()}, normals{Matrix4::Identity()};
    EXPECT_FALSE(BuildSoftBodyPose(data, points, {}, base, models, normals));
    EXPECT_EQ(models.size(), 1u);
    EXPECT_EQ(normals.size(), 1u);
}
