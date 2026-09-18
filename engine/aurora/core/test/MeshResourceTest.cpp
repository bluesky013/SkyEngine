//
// Mesh tests: CPU data interface + Mesh structure + RenderGeometry/Skeleton attach.
//

#include <aurora/resource/Mesh.h>
#include <aurora/resource/MeshData.h>
#include <aurora/resource/RenderGeometry.h>
#include <aurora/rhi/VertexSemantic.h>

#include "AuroraTestHelper.h"

#include <gtest/gtest.h>

using namespace sky::aurora;
using namespace sky::aurora::test;

struct TestVertex {
    float x;
    float y;
    float z;
    float u;
    float v;
};

TEST(MeshResourceTest, RawMeshVertexDataBuild)
{
    TRawMeshVertexData<TestVertex> verts(2);
    EXPECT_EQ(verts.Count(), 2u);
    EXPECT_EQ(verts.GetStride(), sizeof(TestVertex));

    verts.SetVertexData(0, TestVertex{1.f, 2.f, 3.f, 0.f, 0.f});
    verts.SetVertexData(1, TestVertex{4.f, 5.f, 6.f, 1.f, 1.f});

    const auto &v0 = verts.GetVertexData<TestVertex>(0);
    EXPECT_EQ(v0.x, 1.f);
    EXPECT_EQ(v0.v, 0.f);
    const auto &v1 = verts.GetVertexData<TestVertex>(1);
    EXPECT_EQ(v1.x, 4.f);
    EXPECT_EQ(v1.v, 1.f);
}

TEST(MeshResourceTest, RawMeshIndexDataBuild)
{
    RawMeshIndexData idx16(4, IndexType::U16);
    idx16.SetIndex(0, 10);
    idx16.SetIndex(3, 65535);
    EXPECT_EQ(idx16.GetIndex(0), 10u);
    EXPECT_EQ(idx16.GetIndex(3), 65535u);
    EXPECT_EQ(idx16.GetIndexType(), IndexType::U16);

    RawMeshIndexData idx32(4, IndexType::U32);
    idx32.SetIndex(1, 70000);
    EXPECT_EQ(idx32.GetIndex(1), 70000u);
    EXPECT_EQ(idx32.GetIndexType(), IndexType::U32);
}

TEST(MeshResourceTest, MeshStructure)
{
    Mesh mesh(sky::Name("cube"));

    auto geo = sky::CounterPtr<RenderGeometry>(new RenderGeometry(sky::Name("geo")));
    mesh.SetGeometry(geo);
    EXPECT_EQ(mesh.GetGeometry(), geo.Get());

    SubMesh sub;
    sub.firstVertex   = 0;
    sub.vertexCount   = 24;
    sub.firstIndex    = 0;
    sub.indexCount    = 36;
    sub.materialIndex = 0;
    mesh.AddSubMesh(sub);
    ASSERT_EQ(mesh.GetSubMeshes().size(), 1u);
    EXPECT_EQ(mesh.GetSubMeshes()[0].firstVertex, 0u);
    EXPECT_EQ(mesh.GetSubMeshes()[0].vertexCount, 24u);
    EXPECT_EQ(mesh.GetSubMeshes()[0].indexCount, 36u);

    BlendShape shape;
    shape.name = sky::Name("smile");
    shape.positionDeltas.resize(24);
    mesh.AddBlendShape(shape);
    EXPECT_EQ(mesh.GetBlendShapeCount(), 1u);
    EXPECT_EQ(mesh.GetBlendShapes()[0].positionDeltas.size(), 24u);

    EXPECT_EQ(mesh.GetName(), sky::Name("cube"));
    EXPECT_FALSE(mesh.HasSkin());
}

TEST(MeshResourceTest, SkeletonAttach)
{
    Mesh mesh(sky::Name("skinned"));

    auto skel = sky::CounterPtr<Skeleton>(new Skeleton());
    Bone root;
    root.name   = sky::Name("root");
    root.parent = -1;
    skel->AddBone(root);
    Bone child;
    child.name   = sky::Name("child");
    child.parent = 0;
    skel->AddBone(child);

    mesh.SetSkeleton(skel);
    EXPECT_TRUE(mesh.HasSkin());
    ASSERT_EQ(mesh.GetSkeleton()->GetBoneCount(), 2u);
    EXPECT_EQ(mesh.GetSkeleton()->GetBone(0)->name, sky::Name("root"));
    EXPECT_EQ(mesh.GetSkeleton()->GetBone(1)->parent, 0);
    EXPECT_EQ(mesh.GetSkeleton()->GetBones().size(), 2u);
}

TEST(MeshResourceTest, VertexSemanticSkin)
{
    VertexSemantic semantic;
    ASSERT_TRUE(ParseVertexSemantic("JOINTS", semantic));
    EXPECT_EQ(semantic, VertexSemantic::JOINTS);
    ASSERT_TRUE(ParseVertexSemantic("WEIGHTS", semantic));
    EXPECT_EQ(semantic, VertexSemantic::WEIGHTS);
    EXPECT_STREQ(VertexSemanticName(VertexSemantic::JOINTS), "JOINTS");
    EXPECT_STREQ(VertexSemanticName(VertexSemantic::WEIGHTS), "WEIGHTS");
}
