//
// Aurora barrier-layer inference unit tests (no device required).
//

#include <gtest/gtest.h>
#include <aurora/rhi/Barrier.h>

using namespace sky::aurora;

TEST(BarrierInferTest, EmptyAccessReturnsUndefined)
{
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::NONE), ImageLayout::UNDEFINED);
}

TEST(BarrierInferTest, RenderTargetWrite)
{
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::RTV), ImageLayout::COLOR_ATTACHMENT);
}

TEST(BarrierInferTest, DepthStencilWrite)
{
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::DSV), ImageLayout::DEPTH_STENCIL_ATTACHMENT);
}

TEST(BarrierInferTest, DepthStencilReadOnly)
{
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::DSV_READ), ImageLayout::DEPTH_STENCIL_READ_ONLY);
}

TEST(BarrierInferTest, ShaderRead)
{
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::SRV), ImageLayout::SHADER_READ_ONLY);
}

TEST(BarrierInferTest, AnyUavGoesToGeneral)
{
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::UAV), ImageLayout::GENERAL);
}

TEST(BarrierInferTest, TransferDirections)
{
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::COPY_SRC), ImageLayout::TRANSFER_SRC);
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::COPY_DST), ImageLayout::TRANSFER_DST);
}

TEST(BarrierInferTest, Present)
{
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::PRESENT), ImageLayout::PRESENT);
}

TEST(BarrierInferTest, ConflictingAccessFallsToGeneral)
{
    const AccessFlags conflict = AccessFlagBit::RTV | AccessFlagBit::SRV;
    EXPECT_EQ(InferLayoutForAccess(conflict), ImageLayout::GENERAL);
}

TEST(BarrierInferTest, IsLayoutCompatibleWithAccess)
{
    EXPECT_TRUE(IsLayoutCompatibleWithAccess(ImageLayout::COLOR_ATTACHMENT, AccessFlagBit::RTV));
    EXPECT_TRUE(IsLayoutCompatibleWithAccess(ImageLayout::GENERAL, AccessFlagBit::SRV));
    EXPECT_FALSE(IsLayoutCompatibleWithAccess(ImageLayout::COLOR_ATTACHMENT, AccessFlagBit::SRV));
}
