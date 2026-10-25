#include <aurora/renderer/ImportedSkinBinding.h>
#include <aurora/resource/Skin.h>
#include <core/math/Transform.h>
#include <gtest/gtest.h>

#include <limits>

using namespace sky;
using namespace sky::aurora;

TEST(ImportedSkinBinding, ResolvesOnlyCompleteUniqueMeshBindingWithoutChangingFailedOutput)
{
    const auto                  mesh = Uuid::Create();
    const ImportedSkinBinding   first{Uuid::Create(), Uuid::Create()};
    const ImportedSkinBinding   second{Uuid::Create(), Uuid::Create()};
    std::map<std::string, Uuid> defaults{{"mesh", mesh}, {"skin", first.skin}, {"skeleton", first.skeleton}};
    ImportedSkinBinding         result;
    EXPECT_EQ(ResolveImportedSkinBinding(mesh, defaults, result), ImportedSkinBindingStatus::Ready);
    EXPECT_EQ(result, first);
    defaults.insert({{"binding:a:mesh", mesh},
                     {"binding:a:skin", first.skin},
                     {"binding:a:skeleton", first.skeleton},
                     {"binding:b:mesh", mesh},
                     {"binding:b:skin", first.skin},
                     {"binding:b:skeleton", first.skeleton}});
    EXPECT_EQ(ResolveImportedSkinBinding(mesh, defaults, result), ImportedSkinBindingStatus::Ready);
    defaults["binding:b:skin"]     = second.skin;
    defaults["binding:b:skeleton"] = second.skeleton;
    EXPECT_EQ(ResolveImportedSkinBinding(mesh, defaults, result), ImportedSkinBindingStatus::Ambiguous);
    EXPECT_EQ(result, first);
    defaults.erase("binding:b:skin");
    EXPECT_EQ(ResolveImportedSkinBinding(mesh, defaults, result), ImportedSkinBindingStatus::Missing);
    EXPECT_EQ(result, first);
    EXPECT_EQ(ResolveImportedSkinBinding(Uuid::Create(), defaults, result), ImportedSkinBindingStatus::Missing);
    defaults["binding:b:skin"] = {};
    EXPECT_EQ(ResolveImportedSkinBinding(mesh, defaults, result), ImportedSkinBindingStatus::Missing);
}

namespace {
    SkinData TwoSlots()
    {
        SkinData data;
        data.input                      = {17, {"slot0", "slot1"}};
        data.binds                      = {{Matrix4::Identity(), "rig/root", 0}, {Matrix4::Identity(), "rig/root/hand", 0}};
        data.binds[1].inverseBind[3][1] = -2.f;
        return data;
    }

    SkinTargetLayout Rig()
    {
        SkinTargetLayout target;
        target.signature                = 31;
        target.paths                    = {"rig/root", "rig/root/hand"};
        target.referenceModels          = {Matrix4::Identity(), Matrix4::Identity()};
        target.referenceModels[1][3][1] = 2.f;
        return target;
    }

    SkinBindingKey Key(uint64_t signature = 31)
    {
        return {Uuid::CreateWithSeed(1), Uuid::CreateWithSeed(2), Uuid::CreateWithSeed(3), 1, 1, 1, signature,
                Uuid::CreateWithSeed(4), Uuid::CreateWithSeed(5), Uuid::CreateWithSeed(6)};
    }
} // namespace

TEST(SkinBinding, NamedSlotsResolveAfterRigReorder)
{
    const auto data = TwoSlots();
    const auto skin = Skin::Create(data);
    ASSERT_NE(skin, nullptr);
    auto target = Rig();
    std::swap(target.paths[0], target.paths[1]);
    std::swap(target.referenceModels[0], target.referenceModels[1]);
    target.signature = 32;
    ResolvedSkinBindingPtr binding;
    ASSERT_TRUE(ResolveSkinBinding(data.input, *skin, target, Key(32), binding));
    ASSERT_EQ(binding->slotToBone.size(), 2U);
    EXPECT_EQ(binding->slotToBone[0], 1);
    EXPECT_EQ(binding->slotToBone[1], 0);
    std::vector<Matrix4> palette;
    ASSERT_TRUE(BuildSkinningPalette(*binding, target.referenceModels, palette));
    for (const auto &matrix : palette) {
        for (uint32_t column = 0; column < 4; ++column) {
            for (uint32_t row = 0; row < 4; ++row) {
                EXPECT_NEAR(matrix[column][row], column == row ? 1.f : 0.f, 1e-5f);
            }
        }
    }
}

TEST(SkinBinding, OneMeshLayoutSupportsDifferentSkinRigPairs)
{
    const auto data                  = TwoSlots();
    const auto skinA                 = Skin::Create(data);
    auto       dataB                 = data;
    dataB.binds[1].targetBone        = "other/hand";
    dataB.binds[1].inverseBind[3][1] = -5.f;
    const auto skinB                 = Skin::Create(dataB);
    auto       targetB               = Rig();
    targetB.signature                = 77;
    targetB.paths[1]                 = "other/hand";
    targetB.referenceModels[1][3][1] = 5.f;
    auto keyB                        = Key(77);
    keyB.skin                        = Uuid::CreateWithSeed(4);
    keyB.skeleton                    = Uuid::CreateWithSeed(5);
    ResolvedSkinBindingPtr bindingA;
    ResolvedSkinBindingPtr bindingB;
    ASSERT_TRUE(ResolveSkinBinding(data.input, *skinA, Rig(), Key(), bindingA));
    ASSERT_TRUE(ResolveSkinBinding(data.input, *skinB, targetB, keyB, bindingB));
    EXPECT_NE(bindingA->key, bindingB->key);
    auto poseA     = Rig().referenceModels;
    auto poseB     = targetB.referenceModels;
    poseA[1][3][0] = 3.f;
    poseB[1][3][0] = -4.f;
    std::vector<Matrix4> paletteA;
    std::vector<Matrix4> paletteB;
    ASSERT_TRUE(BuildSkinningPalette(*bindingA, poseA, paletteA));
    ASSERT_TRUE(BuildSkinningPalette(*bindingB, poseB, paletteB));
    EXPECT_FLOAT_EQ(paletteA[1][3][0], 3.f);
    EXPECT_FLOAT_EQ(paletteB[1][3][0], -4.f);
    EXPECT_FLOAT_EQ(skinA->GetData().binds[1].inverseBind[3][1], -2.f);

    std::vector<Matrix4> secondInstance;
    ASSERT_TRUE(BuildSkinningPalette(*bindingA, Rig().referenceModels, secondInstance));
    EXPECT_FLOAT_EQ(secondInstance[1][3][0], 0.f);
    EXPECT_FLOAT_EQ(paletteA[1][3][0], 3.f);
}

TEST(SkinBinding, RejectsWrongSlotContractMissingAmbiguousAndWrongReference)
{
    const auto             data = TwoSlots();
    const auto             skin = Skin::Create(data);
    ResolvedSkinBindingPtr binding;
    auto                   input = data.input;
    std::swap(input.slots[0], input.slots[1]);
    EXPECT_EQ(ResolveSkinBinding(input, *skin, Rig(), Key(), binding).error, SkinError::LayoutMismatch);
    auto target     = Rig();
    target.paths[1] = "absent";
    EXPECT_EQ(ResolveSkinBinding(data.input, *skin, target, Key(), binding).error, SkinError::MissingBone);
    target                          = Rig();
    target.referenceModels[1][3][1] = 3.f;
    EXPECT_EQ(ResolveSkinBinding(data.input, *skin, target, Key(), binding).error, SkinError::ReferenceMismatch);
    auto shortName                = data;
    shortName.binds[1].targetBone = "hand";
    const auto ambiguous          = Skin::Create(shortName);
    target                        = Rig();
    target.paths.push_back("another/hand");
    target.referenceModels.push_back(target.referenceModels[1]);
    EXPECT_EQ(ResolveSkinBinding(data.input, *ambiguous, target, Key(), binding).error, SkinError::AmbiguousBone);
    EXPECT_EQ(binding, nullptr);
}

TEST(SkinBinding, IndexedSkinRequiresExactLayoutAndRevisionKey)
{
    auto data = TwoSlots();
    data.mode = SkinBindingMode::Indexed;
    EXPECT_FALSE(Skin::Validate(data));
    data.targetLayoutSignature = 31;
    data.binds[1].targetIndex  = 1;
    const auto skin            = Skin::Create(data);
    ASSERT_NE(skin, nullptr);
    ResolvedSkinBindingPtr binding;
    ASSERT_TRUE(ResolveSkinBinding(data.input, *skin, Rig(), Key(), binding));
    const auto oldBinding = binding;
    auto       target     = Rig();
    target.signature      = 32;
    EXPECT_EQ(ResolveSkinBinding(data.input, *skin, target, Key(32), binding).error, SkinError::LayoutMismatch);
    EXPECT_EQ(binding, oldBinding);
    EXPECT_EQ(ResolveSkinBinding(data.input, *skin, Rig(), Key(32), binding).error, SkinError::InvalidData);
    auto newRevision = Key();
    ++newRevision.skeletonRevision;
    ASSERT_TRUE(ResolveSkinBinding(data.input, *skin, Rig(), newRevision, binding));
    EXPECT_NE(oldBinding->key, binding->key);
}

TEST(SkinBinding, InvalidDataAndSingularPoseDoNotPublishPartialPalette)
{
    auto data                       = TwoSlots();
    data.binds[0].inverseBind[0][0] = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(Skin::Create(data), nullptr);
    data                = TwoSlots();
    data.input.slots[1] = data.input.slots[0];
    EXPECT_FALSE(Skin::Validate(data));
    data                        = TwoSlots();
    const auto             skin = Skin::Create(data);
    ResolvedSkinBindingPtr binding;
    ASSERT_TRUE(ResolveSkinBinding(data.input, *skin, Rig(), Key(), binding));
    std::vector<Matrix4> palette;
    ASSERT_TRUE(BuildSkinningPalette(*binding, Rig().referenceModels, palette));
    auto broken     = Rig().referenceModels;
    broken[1][0][0] = 0.f;
    EXPECT_FALSE(BuildSkinningPalette(*binding, broken, palette));
    EXPECT_EQ(palette.size(), 2U);
    EXPECT_FLOAT_EQ(palette[1][0][0], 1.f);
}

TEST(SkinBinding, CacheSharesOnlyExactGenerationsAndKeepsOldLeasesAlive)
{
    const auto             data = TwoSlots();
    const auto             skin = Skin::Create(data);
    SkinBindingCache       cache;
    ResolvedSkinBindingPtr first, reused, reloaded;
    ASSERT_TRUE(cache.Resolve(data.input, *skin, Rig(), Key(), first));
    ASSERT_TRUE(cache.Resolve(data.input, *skin, Rig(), Key(), reused));
    EXPECT_EQ(first, reused);
    auto changed               = Key();
    changed.skeletonGeneration = Uuid::CreateWithSeed(99);
    ASSERT_TRUE(cache.Resolve(data.input, *skin, Rig(), changed, reloaded));
    EXPECT_NE(first, reloaded);
    auto target = Rig();
    std::swap(target.paths[0], target.paths[1]);
    std::swap(target.referenceModels[0], target.referenceModels[1]);
    target.signature       = 32;
    changed.skeletonLayout = target.signature;
    ++changed.skeletonRevision;
    ASSERT_TRUE(cache.Resolve(data.input, *skin, target, changed, reused));
    EXPECT_EQ(reused->slotToBone[0], 1U);
    EXPECT_EQ(first->slotToBone[0], 0U);
    cache.Clear();
    std::vector<Matrix4> palette;
    ASSERT_TRUE(BuildSkinningPalette(*first, Rig().referenceModels, palette));
    EXPECT_FLOAT_EQ(palette[1][3][1], 0.f);
    ASSERT_TRUE(cache.Resolve(data.input, *skin, Rig(), Key(), reused));
    EXPECT_NE(first, reused);
}

TEST(SkinBinding, SlotBoundsContainBlendedVerticesUnderMirroringAndActorTransform)
{
    const std::vector<SkinSlotBounds> slots{{true, {{-1, -2, -1}, {2, 3, 1}}}, {true, {{-1, -2, -1}, {2, 3, 1}}}, {false, {}}};
    std::vector<Matrix4>              palette(3, Matrix4::Identity());
    palette[0][0].x = -2.f;
    palette[0][3].x = 4.f;
    palette[1][1].y = 3.f;
    palette[1][3].x = -3.f;
    AABB bounds;
    ASSERT_TRUE(BuildSkinnedBounds(slots, palette, bounds));
    Matrix4 world          = Matrix4::Identity();
    world[0].x             = -1.f;
    world[3].y             = 9.f;
    const auto worldBounds = AABB::Transform(bounds, world);
    for (uint32_t corner = 0; corner < 8; ++corner) {
        const Vector4 vertex{corner & 1 ? 2.f : -1.f, corner & 2 ? 3.f : -2.f, corner & 4 ? 1.f : -1.f, 1.f};
        for (float weight : {0.f, 0.25f, 0.75f, 1.f}) {
            const auto position = world * ((palette[0] * vertex) * weight + (palette[1] * vertex) * (1.f - weight));
            for (uint32_t axis = 0; axis < 3; ++axis) {
                EXPECT_GE(position[axis] + 1e-5f, worldBounds.min[axis]);
                EXPECT_LE(position[axis] - 1e-5f, worldBounds.max[axis]);
            }
        }
    }
    const auto previous = bounds;
    palette[0][0].x     = 0.f;
    EXPECT_FALSE(BuildSkinnedBounds(slots, palette, bounds));
    EXPECT_EQ(previous.min, bounds.min);
    EXPECT_EQ(previous.max, bounds.max);
}
