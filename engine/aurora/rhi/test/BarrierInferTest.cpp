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

TEST(BarrierInferTest, ColorWrite)
{
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::COLOR_WRITE), ImageLayout::COLOR_ATTACHMENT);
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::COLOR_INOUT_WRITE), ImageLayout::COLOR_ATTACHMENT);
}

TEST(BarrierInferTest, DepthWrite)
{
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::DEPTH_STENCIL_WRITE), ImageLayout::DEPTH_STENCIL_ATTACHMENT);
}

TEST(BarrierInferTest, DepthReadOnly)
{
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::DEPTH_STENCIL_READ), ImageLayout::DEPTH_STENCIL_READ_ONLY);
}

TEST(BarrierInferTest, ShaderReadFromAnySrvStage)
{
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::FRAGMENT_SRV), ImageLayout::SHADER_READ_ONLY);
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::VERTEX_SRV),   ImageLayout::SHADER_READ_ONLY);
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::COMPUTE_SRV),  ImageLayout::SHADER_READ_ONLY);
    AccessFlags multi = AccessFlagBit::VERTEX_SRV | AccessFlagBit::FRAGMENT_SRV;
    EXPECT_EQ(InferLayoutForAccess(multi), ImageLayout::SHADER_READ_ONLY);
}

TEST(BarrierInferTest, AnyUavGoesToGeneral)
{
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::COMPUTE_UAV_WRITE), ImageLayout::GENERAL);
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::FRAGMENT_UAV_READ), ImageLayout::GENERAL);
}

TEST(BarrierInferTest, TransferDirections)
{
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::TRANSFER_READ),  ImageLayout::TRANSFER_SRC);
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::TRANSFER_WRITE), ImageLayout::TRANSFER_DST);
}

TEST(BarrierInferTest, Present)
{
    EXPECT_EQ(InferLayoutForAccess(AccessFlagBit::PRESENT), ImageLayout::PRESENT);
}

TEST(BarrierInferTest, ConflictingAccessFallsToGeneral)
{
    AccessFlags conflict = AccessFlagBit::COLOR_WRITE | AccessFlagBit::FRAGMENT_SRV;
    EXPECT_EQ(InferLayoutForAccess(conflict), ImageLayout::GENERAL);
}

TEST(BarrierInferTest, IsLayoutCompatibleWithAccess)
{
    EXPECT_TRUE(IsLayoutCompatibleWithAccess(ImageLayout::COLOR_ATTACHMENT, AccessFlagBit::COLOR_WRITE));
    EXPECT_TRUE(IsLayoutCompatibleWithAccess(ImageLayout::GENERAL, AccessFlagBit::FRAGMENT_SRV));
    EXPECT_FALSE(IsLayoutCompatibleWithAccess(ImageLayout::COLOR_ATTACHMENT, AccessFlagBit::FRAGMENT_SRV));
}
