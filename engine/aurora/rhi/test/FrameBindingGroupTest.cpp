//
// Created on 2026/10/27.
//

#include <algorithm>
#include <aurora/rdg/FrameBindings.h>
#include <gtest/gtest.h>

using namespace sky::aurora;

namespace {
    bool Equivalent(const FrameBindingGroup *left, const FrameBindingGroup *right)
    {
        const FrameBindingGroupLess less;
        return !less(left, right) && !less(right, left);
    }
    FrameBindingGroupData GroupData(const ShaderPtr &shader)
    {
        FrameBindingGroupData data;
        data.layoutOwner = shader;
        data.set         = 2;
        data.buffers     = {{9, {{17, 0}, 32, 16}, FrameBufferBindingKind::STORAGE}, {1, {{17, 1}, 0, 16}, FrameBufferBindingKind::UNIFORM}};
        data.images      = {{4, {17, 0}, {}, FrameImageBindingKind::SAMPLED}};
        return data;
    }
} // namespace

TEST(FrameBindingGroups, CanonicalValuesShareKeysWhileResourceLayoutsAndRangesRemainDistinct)
{
    ShaderPtr         shader(new Shader);
    auto              data = GroupData(shader);
    FrameBindingGroup original(data);
    std::reverse(data.buffers.begin(), data.buffers.end());
    FrameBindingGroup reordered(data);
    EXPECT_TRUE(Equivalent(&original, &reordered));
    ASSERT_EQ(reordered.GetData().buffers.size(), 2u);
    EXPECT_EQ(reordered.GetData().buffers.front().binding, 1u);

    data.buffers.front().range.offset += 16;
    FrameBindingGroup changedRange(data);
    EXPECT_FALSE(Equivalent(&original, &changedRange));
    data                             = GroupData(shader);
    data.images.front().view.baseMip = 1;
    FrameBindingGroup changedView(data);
    EXPECT_FALSE(Equivalent(&original, &changedView));
    data = GroupData(ShaderPtr(new Shader));
    FrameBindingGroup changedOwner(data);
    EXPECT_FALSE(Equivalent(&original, &changedOwner));
}

TEST(FrameBindingGroups, AccessAndDynamicOffsetsBelongToTheUseAndResolveEffectiveRanges)
{
    FrameComputePass pass;
    pass.shader               = ShaderPtr(new Shader);
    pass.groups[2].group      = std::make_shared<const FrameBindingGroup>(GroupData(pass.shader));
    const auto snapshot       = pass.groups[2].group;
    pass.groups[2].bufferUses = {{9, 64, FrameAccessMode::WRITE}};
    ASSERT_FALSE(ValidateBindingGroups(pass.shader.Get(), pass.groups));
    auto first = ResolveComputeBindings(pass);
    ASSERT_EQ(first.buffers.size(), 2u);
    EXPECT_EQ(first.buffers[1].dynamicOffset, 64u);
    EXPECT_EQ(first.buffers[1].access, FrameAccessMode::WRITE);
    auto range = ResolveFrameBufferRange(first.buffers[1], 256);
    ASSERT_TRUE(std::holds_alternative<RasterBufferRange>(range));
    EXPECT_EQ(std::get<RasterBufferRange>(range).offset, 96u);

    pass.groups[2].bufferUses = {{9, 128, FrameAccessMode::READ_WRITE}};
    EXPECT_EQ(pass.groups[2].group, snapshot);
    EXPECT_TRUE(Equivalent(snapshot.get(), pass.groups[2].group.get()));
    const auto second = ResolveComputeBindings(pass);
    EXPECT_EQ(second.buffers[1].dynamicOffset, 128u);
    EXPECT_EQ(second.buffers[1].access, FrameAccessMode::READ_WRITE);
    EXPECT_EQ(snapshot->GetData().buffers[1].range.offset, 32u);
}

TEST(FrameBindingGroups, InvalidOwnerAndUnknownOrDuplicateOverridesAreRejected)
{
    FrameComputePass pass;
    pass.shader          = ShaderPtr(new Shader);
    pass.groups[2].group = std::make_shared<const FrameBindingGroup>(GroupData(pass.shader));
    const auto owner     = pass.shader;
    pass.shader          = ShaderPtr(new Shader);
    EXPECT_TRUE(ValidateBindingGroups(pass.shader.Get(), pass.groups));
    pass.shader               = owner;
    pass.groups[2].bufferUses = {{9, 16}, {9, 32}};
    EXPECT_TRUE(ValidateBindingGroups(pass.shader.Get(), pass.groups));
    pass.groups[2].bufferUses = {{4, 16}};
    EXPECT_TRUE(ValidateBindingGroups(pass.shader.Get(), pass.groups));
    pass.groups[2].bufferUses.clear();
    pass.groups[2].imageUses = {{9, FrameAccessMode::READ}};
    EXPECT_TRUE(ValidateBindingGroups(pass.shader.Get(), pass.groups));
    pass.groups[2]            = {};
    pass.groups[2].bufferUses = {{9, 16}};
    EXPECT_TRUE(ValidateBindingGroups(pass.shader.Get(), pass.groups));
}

TEST(FrameBindingGroups, FailedSetLeavesThePreviouslyPublishedGroupsUnchanged)
{
    RasterDraw draw;
    draw.shader = ShaderPtr(new Shader);
    RasterBindingDraft draft;
    draft.uniforms = {{2, 1, {{17, 0}, 0, 16}, 32}};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetRasterBindings(draw, draft)));
    const auto snapshot = draw.groups[2].group;
    draft.images        = {{2, 1, {17, 0}, {}}};
    EXPECT_TRUE(std::holds_alternative<FrameError>(SetRasterBindings(draw, draft)));
    EXPECT_EQ(draw.groups[2].group, snapshot);
    EXPECT_EQ(draw.groups[2].bufferUses.front().dynamicOffset, 32u);
    draft.images.clear();
    draft.uniforms.front().set = MAX_SHADER_SETS;
    EXPECT_TRUE(std::holds_alternative<FrameError>(SetRasterBindings(draw, std::move(draft))));
    EXPECT_EQ(draw.groups[2].group, snapshot);
}
