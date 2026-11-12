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

TEST(FrameBindingGroups, EnumBoundariesAndOversizedDraftsPreservePublishedGroups)
{
    FrameComputePass pass;
    pass.shader = ShaderPtr(new Shader);
    ComputeBindingDraft draft;
    draft.buffers = {{0, 0, {{1, 0}, 0, 16}, 0, FrameAccessMode::READ_WRITE, FrameBufferBindingKind::STORAGE}};
    draft.images  = {{0, 1, {1, 0}, {}, FrameAccessMode::READ_WRITE, FrameImageBindingKind::STORAGE}};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetComputeBindings(pass, draft)));
    const auto group = pass.groups[0].group;
    for (const auto value : {uint8_t(FrameBufferBindingKind::END), uint8_t(UINT8_MAX)}) {
        auto invalid            = draft;
        invalid.buffers[0].kind = static_cast<FrameBufferBindingKind>(value);
        EXPECT_TRUE(std::holds_alternative<FrameError>(SetComputeBindings(pass, invalid)));
        EXPECT_EQ(pass.groups[0].group, group);
        auto data            = group->GetData();
        data.buffers[0].kind = static_cast<FrameBufferBindingKind>(value);
        auto groups          = pass.groups;
        groups[0].group      = std::make_shared<const FrameBindingGroup>(std::move(data));
        EXPECT_TRUE(ValidateBindingGroups(pass.shader.Get(), groups));
    }
    for (const auto value : {uint8_t(FrameImageBindingKind::END), uint8_t(UINT8_MAX)}) {
        auto invalid           = draft;
        invalid.images[0].kind = static_cast<FrameImageBindingKind>(value);
        EXPECT_TRUE(std::holds_alternative<FrameError>(SetComputeBindings(pass, invalid)));
        EXPECT_EQ(pass.groups[0].group, group);
        auto data           = group->GetData();
        data.images[0].kind = static_cast<FrameImageBindingKind>(value);
        auto groups         = pass.groups;
        groups[0].group     = std::make_shared<const FrameBindingGroup>(std::move(data));
        EXPECT_TRUE(ValidateBindingGroups(pass.shader.Get(), groups));
    }
    for (const auto access : {FrameAccessMode::END, static_cast<FrameAccessMode>(UINT8_MAX)}) {
        auto invalid              = draft;
        invalid.buffers[0].access = access;
        EXPECT_TRUE(std::holds_alternative<FrameError>(SetComputeBindings(pass, invalid)));
        invalid                  = draft;
        invalid.images[0].access = access;
        EXPECT_TRUE(std::holds_alternative<FrameError>(SetComputeBindings(pass, invalid)));
        EXPECT_EQ(pass.groups[0].group, group);
        auto groups                    = pass.groups;
        groups[0].bufferUses[0].access = access;
        EXPECT_TRUE(ValidateBindingGroups(pass.shader.Get(), groups));
        groups                        = pass.groups;
        groups[0].imageUses[0].access = access;
        EXPECT_TRUE(ValidateBindingGroups(pass.shader.Get(), groups));
    }
    draft.images.clear();
    const auto duplicate = draft.buffers.front();
    draft.buffers.assign(MAX_SHADER_SETS * 32 + 1, duplicate);
    EXPECT_TRUE(std::holds_alternative<FrameError>(SetComputeBindings(pass, draft)));
    EXPECT_EQ(pass.groups[0].group, group);
    EXPECT_EQ(pass.groups[0].bufferUses[0].access, FrameAccessMode::READ_WRITE);
}

TEST(FrameBindingGroups, OversizedRasterDraftFailsBeforeExpansionAndPreservesPublishedGroups)
{
    class CountingShader final : public Shader {
    public:
        const ShaderReflection &GetReflection() const override
        {
            ++reflectionReads;
            return reflection;
        }
        mutable uint32_t reflectionReads = 0;
        ShaderReflection reflection;
    };
    auto      *shader = new CountingShader;
    RasterDraw draw;
    draw.shader = ShaderPtr(shader);
    RasterBindingDraft draft;
    draft.uniforms = {{0, 0, {{1, 0}, 0, 16}, 16}};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetRasterBindings(draw, draft)));
    const auto group   = draw.groups[0].group;
    const auto binding = draft.uniforms.front();
    draft.uniforms.assign(MAX_SHADER_SETS * 32 + 1, binding);
    shader->reflectionReads = 0;
    const auto rejected     = SetRasterBindings(draw, draft);
    ASSERT_TRUE(std::holds_alternative<FrameError>(rejected));
    EXPECT_EQ(shader->reflectionReads, 0u);
    EXPECT_NE(std::get<FrameError>(rejected).message.find("declaration budget"), std::string::npos);
    EXPECT_EQ(draw.groups[0].group, group);
    ASSERT_EQ(draw.groups[0].bufferUses.size(), 1u);
    EXPECT_EQ(draw.groups[0].bufferUses[0].dynamicOffset, 16u);
    draw.shader = nullptr;
    EXPECT_TRUE(std::holds_alternative<FrameError>(SetRasterBindings(draw, {})));
    EXPECT_EQ(draw.groups[0].group, group);
}
