//
// BuiltinGeometry tests: Build assembles separate streams + index + bounds.
//

#include <aurora/resource/BuiltinGeometry.h>
#include <core/math/GeometryGenerator.h>

#include "AuroraTestHelper.h"

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

TEST_F(AuroraVulkanTest, BuildCube)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    auto streams = GenerateCube(1.0f);
    auto geo = BuiltinGeometry::Build(device, streams, IndexType::U32);
    ASSERT_NE(geo, nullptr);
    ASSERT_EQ(geo->GetVertexStreams().size(), 4u);

    EXPECT_TRUE(geo->GetVertexStreams()[0]->GetLayout().semantics.Test(VertexSemantic::POSITION));
    EXPECT_TRUE(geo->GetVertexStreams()[1]->GetLayout().semantics.Test(VertexSemantic::NORMAL));
    EXPECT_TRUE(geo->GetVertexStreams()[2]->GetLayout().semantics.Test(VertexSemantic::TANGENT));
    EXPECT_TRUE(geo->GetVertexStreams()[3]->GetLayout().semantics.Test(VertexSemantic::UV1));

    EXPECT_NE(geo->GetIndexBuffer(), nullptr);
    EXPECT_EQ(geo->GetIndexBuffer()->GetIndexType(), IndexType::U32);
    EXPECT_GE(geo->GetLocalBounds().max.x, 0.5f);
    EXPECT_LE(geo->GetLocalBounds().min.x, -0.5f);
}

TEST_F(AuroraVulkanTest, BuildU16OverflowReturnsNull)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    // 257*257 = 66049 vertices >= 65536
    auto streams = GenerateSphere(1.0f, 256, 256);
    auto geo = BuiltinGeometry::Build(device, streams, IndexType::U16);
    EXPECT_EQ(geo, nullptr);
}
