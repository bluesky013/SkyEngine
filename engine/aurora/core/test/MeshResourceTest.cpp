//
// Mesh tests: CPU data interface + Mesh structure + RenderGeometry/Skin attach.
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

TEST(MeshResourceTest, SkinAttach)
{
    Mesh mesh(sky::Name("skinned"));

    auto skin = sky::CounterPtr<Skin>(new Skin());
    skin->SetInverseBindMatrices({sky::Matrix4::Identity(), sky::Matrix4::Identity()});
    skin->SetBoneMatrices({sky::Matrix4::Identity(), sky::Matrix4::Identity()});
    skin->SetBoneMapping({0, 1});

    mesh.SetSkin(skin);
    EXPECT_TRUE(mesh.HasSkin());
    ASSERT_NE(mesh.GetSkin(), nullptr);
    EXPECT_EQ(mesh.GetSkin()->GetInverseBindMatrices().size(), 2u);
    EXPECT_EQ(mesh.GetSkin()->GetBoneMatrices().size(), 2u);
    EXPECT_EQ(mesh.GetSkin()->GetBoneMapping().size(), 2u);
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
