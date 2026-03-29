//
// Created by Zach Lee on 2025/10/13.
//
#include <gtest/gtest.h>
#include <render/resource/StaticMesh.h>

using namespace sky;

TEST(StaticMeshTest, StaticMesthCreate01)
{
    StaticMeshGeometry mesh;

    mesh.Init(4, 6, rhi::IndexType::U16);

    mesh.SetPosition(0, Vector3(1.f, 0.f, 0.f));
    mesh.SetPosition(1, Vector3(2.f, 0.f, 0.f));
    mesh.SetPosition(2, Vector3(3.f, 3.f, 0.f));
    mesh.SetPosition(3, Vector3(0.f, 0.f, 4.f));

    mesh.SetIndex(0, 0);
    mesh.SetIndex(1, 1);
    mesh.SetIndex(2, 2);
    mesh.SetIndex(3, 2);
    mesh.SetIndex(4, 3);
    mesh.SetIndex(5, 0);

    auto *positionBuffer = mesh.GetPositionBuffer();

    ASSERT_NE(positionBuffer, nullptr);
    ASSERT_EQ(positionBuffer->Num(), 4u);

    const auto& v1 = positionBuffer->GetVertexData<Vector3>(0);
    ASSERT_FLOAT_EQ(v1.x, 1.f);

    const auto& v2 = positionBuffer->GetVertexData<Vector3>(1);
    ASSERT_FLOAT_EQ(v2.x, 2.f);

    const auto& v3 = positionBuffer->GetVertexData<Vector3>(2);
    ASSERT_FLOAT_EQ(v3.x, 3.f);
    ASSERT_FLOAT_EQ(v3.y, 3.f);

    const auto& v4 = positionBuffer->GetVertexData<Vector3>(3);
    ASSERT_FLOAT_EQ(v4.z, 4.f);

    auto *indexBuffer = mesh.GetIndexBuffer();

    ASSERT_NE(indexBuffer, nullptr);
    ASSERT_EQ(indexBuffer->Num(), 6u);

    ASSERT_EQ(indexBuffer->GetIndexU32(0), 0u);
    ASSERT_EQ(indexBuffer->GetIndexU32(1), 1u);
    ASSERT_EQ(indexBuffer->GetIndexU32(2), 2u);
    ASSERT_EQ(indexBuffer->GetIndexU32(3), 2u);
    ASSERT_EQ(indexBuffer->GetIndexU32(4), 3u);
    ASSERT_EQ(indexBuffer->GetIndexU32(5), 0u);
}//}