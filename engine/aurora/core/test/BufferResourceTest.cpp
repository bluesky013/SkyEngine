//
// Buffer resource tests: compile + basic metadata of the high-level buffer
// types. Actual device creation/upload is exercised by the RHI upload tests.
//

#include <aurora/resource/Buffer.h>

#include "AuroraTestHelper.h"

#include <gtest/gtest.h>

using namespace sky::aurora;
using namespace sky::aurora::test;

TEST(BufferResourceTest, KindUsageBits)
{
    static_assert((VertexBufferKind::Usage & BufferUsageFlagBit::VERTEX).TestBit(BufferUsageFlagBit::VERTEX));
    static_assert((IndexBufferKind::Usage & BufferUsageFlagBit::INDEX).TestBit(BufferUsageFlagBit::INDEX));
    static_assert((UniformBufferKind::Usage & BufferUsageFlagBit::UNIFORM).TestBit(BufferUsageFlagBit::UNIFORM));

    static_assert((StorageBufferKind::Usage & BufferUsageFlagBit::STORAGE).TestBit(BufferUsageFlagBit::STORAGE));
    static_assert((StorageBufferKind::Usage & BufferUsageFlagBit::VERTEX).TestBit(BufferUsageFlagBit::VERTEX));
    static_assert((StorageBufferKind::Usage & BufferUsageFlagBit::INDEX).TestBit(BufferUsageFlagBit::INDEX));
    static_assert((StorageBufferKind::Usage & BufferUsageFlagBit::INDIRECT).TestBit(BufferUsageFlagBit::INDIRECT));
}

TEST(BufferResourceTest, ConcreteTypes)
{
    VertexBuffer<>  vb;
    IndexBuffer<>   ib;
    UniformBuffer<> ub;
    StorageBuffer<> sb;

    VertexLayout layout;
    layout.stride = 32;
    layout.semantics.Set(VertexSemantic::POSITION);
    vb.SetLayout(layout);
    EXPECT_EQ(vb.GetLayout().stride, 32u);

    ib.SetIndexType(IndexType::U32);
    EXPECT_EQ(ib.GetIndexType(), IndexType::U32);

    EXPECT_FALSE(vb.IsCreated());
    EXPECT_EQ(vb.GetBuffer(), nullptr);
    EXPECT_EQ(sb.AsVertex(0), nullptr);
}

// DynamicBuffer inflight ring: two in-flight frames cycle between two
// distinct buffers, then wrap back.
TEST_F(AuroraVulkanTest, DynamicBufferRing)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    UniformBuffer<> ub;
    ASSERT_TRUE(ub.Init(device, 256, 2));

    // Map() triggers lazy creation of the whole ring.
    ASSERT_NE(ub.Map(), nullptr);

    auto *b0 = ub.GetBuffer();
    ASSERT_NE(b0, nullptr);

    ub.AdvanceFrame();
    auto *b1 = ub.GetBuffer();
    ASSERT_NE(b1, nullptr);
    EXPECT_NE(b1, b0);

    ub.AdvanceFrame();
    EXPECT_EQ(ub.GetBuffer(), b0);
}

// DynamicBuffer (CPU_TO_GPU) upload round-trip: Upload writes directly to the
// persistent-mapped buffer, then Map() reads it back.
TEST_F(AuroraVulkanTest, UniformBufferUploadRoundTrip)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    UniformBuffer<> ub;
    ASSERT_TRUE(ub.Init(device, 256));

    uint32_t src[64];
    for (uint32_t i = 0; i < 64; ++i) {
        src[i] = i * 5;
    }

    ASSERT_TRUE(ub.Upload(src, sizeof(src)));
    EXPECT_TRUE(ub.IsCreated());

    auto *mapped = ub.Map();
    ASSERT_NE(mapped, nullptr);
    const auto *readback = reinterpret_cast<const uint32_t *>(mapped);
    for (uint32_t i = 0; i < 64; ++i) {
        EXPECT_EQ(readback[i], i * 5);
    }
}

// StaticBuffer (GPU_ONLY) upload smoke: Upload lazily creates the underlying
// buffer and delegates the copy to the transfer queue.
TEST_F(AuroraVulkanTest, VertexBufferUploadSmoke)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    VertexBuffer<> vb;
    ASSERT_TRUE(vb.Init(device, 256));

    std::vector<uint8_t> data(256, 0x42);
    ASSERT_TRUE(vb.Upload(data.data(), data.size()));

    EXPECT_TRUE(vb.IsCreated());
    EXPECT_NE(vb.GetBuffer(), nullptr);
}

// Named resource smoke: name is carried through Create() and set on the
// backend object (when SKY_ENABLE_RESOURCE_NAME is on).
TEST_F(AuroraVulkanTest, NamedResourceSmoke)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    VertexBuffer<> vb(sky::Name("test_vb"));
    ASSERT_TRUE(vb.Init(device, 128));

    std::vector<uint8_t> data(128, 0x11);
    ASSERT_TRUE(vb.Upload(data.data(), data.size()));

    EXPECT_TRUE(vb.IsCreated());
    EXPECT_NE(vb.GetBuffer(), nullptr);
}
