//
// RenderGeometry tests: composite metadata + ownership smoke.
//

#include <aurora/resource/Buffer.h>
#include <aurora/resource/RenderGeometry.h>

#include "AuroraTestHelper.h"

#include <gtest/gtest.h>

using namespace sky::aurora;
using namespace sky::aurora::test;

TEST(RenderGeometryTest, CompositeMetadata)
{
    auto vb = std::make_unique<VertexBuffer>();
    VertexLayout layout;
    layout.stride = 32;
    layout.semantics.Set(VertexSemantic::POSITION);
    layout.semantics.Set(VertexSemantic::NORMAL);
    vb->SetLayout(layout);

    auto ib = std::make_unique<IndexBuffer>();
    ib->SetIndexType(IndexType::U32);

    RenderGeometry geo(sky::Name("geo"));
    geo.AddVertexStream(std::move(vb));
    geo.SetIndexBuffer(std::move(ib));
    geo.SetLocalBounds(sky::AABB(sky::VEC3_ZERO, sky::VEC3_ONE));

    ASSERT_EQ(geo.GetVertexStreams().size(), 1u);
    EXPECT_EQ(geo.GetVertexStreams()[0]->GetLayout().stride, 32u);
    EXPECT_TRUE(geo.GetVertexStreams()[0]->GetLayout().semantics.Test(VertexSemantic::POSITION));
    EXPECT_NE(geo.GetIndexBuffer(), nullptr);
    EXPECT_EQ(geo.GetIndexBuffer()->GetIndexType(), IndexType::U32);
    EXPECT_EQ(geo.GetLocalBounds().max.x, 1.0f);
    EXPECT_EQ(geo.GetName(), sky::Name("geo"));
}

TEST_F(AuroraVulkanTest, RenderGeometryOwnsUploadedBuffers)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    auto vb = std::make_unique<VertexBuffer>();
    ASSERT_TRUE(vb->Init(device, 256));
    std::vector<uint8_t> vdata(256, 0x42);
    ASSERT_TRUE(vb->Upload(vdata.data(), vdata.size()));

    auto ib = std::make_unique<IndexBuffer>();
    ASSERT_TRUE(ib->Init(device, 128));
    std::vector<uint8_t> idata(128, 0x11);
    ASSERT_TRUE(ib->Upload(idata.data(), idata.size()));

    RenderGeometry geo(sky::Name("geo_upload"));
    geo.AddVertexStream(std::move(vb));
    geo.SetIndexBuffer(std::move(ib));

    ASSERT_EQ(geo.GetVertexStreams().size(), 1u);
    EXPECT_NE(geo.GetVertexStreams()[0]->GetBuffer(), nullptr);
    EXPECT_NE(geo.GetIndexBuffer()->GetBuffer(), nullptr);

    // Destroying the geometry releases the underlying rhi buffers via unique_ptr.
}
