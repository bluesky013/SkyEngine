#include <gtest/gtest.h>
#include <softbody/assets/SoftBodyAsset.h>

using namespace sky;
using namespace sky::softbody;

namespace {
    SoftBodyAssetData Product()
    {
        SoftBodyAssetData value;
        value.mesh                 = Uuid::Create();
        value.skin                 = Uuid::Create();
        value.skeleton             = Uuid::Create();
        value.skeletonLayout       = 7;
        value.controlIds           = {"a", "b", "c", "d"};
        value.body.mesh.vertices   = {{{0, 0, 0}}, {{1, 0, 0}}, {{0, 1, 0}}, {{0, 0, 1}}};
        value.body.mesh.edges      = {{{0, 1}}, {{0, 2}}, {{0, 3}}, {{1, 2}}, {{1, 3}}, {{2, 3}}};
        value.body.mesh.tetrahedra = {{{0, 1, 2, 3}, 1e-6f}};
        value.body.mesh.faces      = {{1, 2, 3}, {0, 3, 2}, {0, 1, 3}, {0, 2, 1}};
        return value;
    }
    void SetU32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value)
    {
        for (unsigned i = 0; i < 4; ++i)
            bytes[offset + i] = static_cast<uint8_t>(value >> (8 * i));
    }
} // namespace

TEST(SoftBodyAsset, RoundTripPreservesNativeSettingsAndLastValidAttachmentMode)
{
    auto source                              = Product();
    source.body.mesh.vertices[0].mode        = phy::SoftBodyAttachmentMode::RootKinematic;
    source.body.mesh.vertices[0].inverseMass = 0;
    source.body.mesh.vertices[1].mode        = phy::SoftBodyAttachmentMode::WorldFixed;
    source.body.mesh.vertices[1].inverseMass = 0;
    source.body.pressure                     = 2.5f;
    source.body.filter                       = {7, 11};
    source.body.solverIterations             = 9;
    source.body.rootFollow                   = {{2, .02f}, {3, .001f}};
    std::vector<uint8_t> bytes;
    ASSERT_TRUE(EncodeSoftBodyAsset(source, bytes));
    SoftBodyAssetData decoded;
    ASSERT_TRUE(DecodeSoftBodyAsset(bytes, decoded));
    EXPECT_EQ(decoded.mesh, source.mesh);
    EXPECT_EQ(decoded.skin, source.skin);
    EXPECT_EQ(decoded.skeleton, source.skeleton);
    EXPECT_EQ(decoded.skeletonLayout, source.skeletonLayout);
    EXPECT_EQ(decoded.controlIds, source.controlIds);
    EXPECT_EQ(decoded.body.mesh.vertices[0].mode, phy::SoftBodyAttachmentMode::RootKinematic);
    EXPECT_EQ(decoded.body.mesh.vertices[1].mode, phy::SoftBodyAttachmentMode::WorldFixed);
    EXPECT_FLOAT_EQ(decoded.body.pressure, 2.5f);
    EXPECT_EQ(decoded.body.filter.mask, 11u);
    EXPECT_EQ(decoded.body.solverIterations, 9u);
    ASSERT_EQ(decoded.body.rootFollow.size(), 2u);
    EXPECT_EQ(decoded.body.rootFollow[0].vertex, 2u);
    EXPECT_FLOAT_EQ(decoded.body.rootFollow[1].compliance, .001f);
    std::vector<uint8_t> again;
    ASSERT_TRUE(EncodeSoftBodyAsset(decoded, again));
    EXPECT_EQ(again, bytes);
}

TEST(SoftBodyAsset, NormalPolicyVersionTwoAndLegacyVersionOneRemainDistinct)
{
    auto source         = Product();
    source.normalPolicy = SoftBodyNormalPolicy::StandardSkinning;
    std::vector<uint8_t> bytes;
    ASSERT_TRUE(EncodeSoftBodyAsset(source, bytes));
    SoftBodyAssetData loaded;
    ASSERT_TRUE(DecodeSoftBodyAsset(bytes, loaded));
    EXPECT_EQ(loaded.version, 2u);
    EXPECT_EQ(loaded.normalPolicy, SoftBodyNormalPolicy::StandardSkinning);
    const auto sentinel = loaded.mesh;
    for (const uint32_t policy : {static_cast<uint32_t>(SoftBodyNormalPolicy::END), 256u, UINT32_MAX}) {
        auto bad = bytes;
        SetU32(bad, bad.size() - 4, policy);
        EXPECT_FALSE(DecodeSoftBodyAsset(bad, loaded));
        EXPECT_EQ(loaded.mesh, sentinel);
        EXPECT_EQ(loaded.normalPolicy, SoftBodyNormalPolicy::StandardSkinning);
    }
    source.version = 1;
    EXPECT_FALSE(EncodeSoftBodyAsset(source, bytes));
    source.normalPolicy = SoftBodyNormalPolicy::TetGradient;
    ASSERT_TRUE(EncodeSoftBodyAsset(source, bytes));
    ASSERT_TRUE(DecodeSoftBodyAsset(bytes, loaded));
    EXPECT_EQ(loaded.version, 1u);
    EXPECT_EQ(loaded.normalPolicy, SoftBodyNormalPolicy::TetGradient);
}

TEST(SoftBodyAsset, MalformedProductsPreservePreviousOutput)
{
    const auto           source = Product();
    std::vector<uint8_t> valid;
    ASSERT_TRUE(EncodeSoftBodyAsset(source, valid));
    auto       out      = Product();
    const auto sentinel = out.mesh;
    for (size_t length = 0; length < valid.size(); ++length) {
        EXPECT_FALSE(DecodeSoftBodyAsset(std::span(valid.data(), length), out));
        EXPECT_EQ(out.mesh, sentinel);
    }
    for (const auto mode : {static_cast<uint32_t>(phy::SoftBodyAttachmentMode::END), 256u, 0xffffffffu}) {
        auto invalid = valid;
        SetU32(invalid, 140, mode);
        EXPECT_FALSE(DecodeSoftBodyAsset(invalid, out));
        EXPECT_EQ(out.mesh, sentinel);
    }
    for (const auto offset : {4u, 104u, 108u, 144u}) {
        auto invalid = valid;
        SetU32(invalid, offset, 0xffffffffu);
        EXPECT_FALSE(DecodeSoftBodyAsset(invalid, out));
        EXPECT_EQ(out.mesh, sentinel);
    }
    auto trailing = valid;
    trailing.push_back(0);
    EXPECT_FALSE(DecodeSoftBodyAsset(trailing, out));
    EXPECT_EQ(out.mesh, sentinel);
}

TEST(SoftBodyAsset, ValidatesProductIdentityTransformAndControlMapping)
{
    const auto           valid = Product();
    std::vector<uint8_t> out{1, 2, 3};
    auto                 invalid = valid;
    invalid.skin                 = invalid.mesh;
    EXPECT_FALSE(EncodeSoftBodyAsset(invalid, out));
    invalid               = valid;
    invalid.controlIds[1] = invalid.controlIds[0];
    EXPECT_FALSE(EncodeSoftBodyAsset(invalid, out));
    invalid                            = valid;
    invalid.body.transform.translation = {1, 0, 0};
    EXPECT_FALSE(EncodeSoftBodyAsset(invalid, out));
    invalid = valid;
    invalid.body.mesh.vertices.push_back({{2, 2, 2}});
    invalid.controlIds.push_back("isolated");
    EXPECT_FALSE(EncodeSoftBodyAsset(invalid, out));
    EXPECT_EQ(out, (std::vector<uint8_t>{1, 2, 3}));
}

TEST(SoftBodyAsset, Allows255ParticlesPlusRootAndRejects256Particles)
{
    auto value = Product();
    while (value.body.mesh.vertices.size() < 255) {
        const auto i = static_cast<uint32_t>(value.body.mesh.vertices.size());
        value.body.mesh.vertices.push_back({{1.f + float(i), 0, 0}});
        value.body.mesh.tetrahedra.push_back({{0, i, 2, 3}});
        value.controlIds.push_back("point" + std::to_string(i));
    }
    ASSERT_TRUE(ValidateSoftBodyAsset(value));
    std::vector<uint8_t> bytes;
    ASSERT_TRUE(EncodeSoftBodyAsset(value, bytes));
    SoftBodyAssetData decoded;
    ASSERT_TRUE(DecodeSoftBodyAsset(bytes, decoded));
    EXPECT_EQ(decoded.body.mesh.vertices.size(), 255u);
    value.body.mesh.vertices.push_back({{300, 0, 0}});
    value.body.mesh.tetrahedra.push_back({{0, 255, 2, 3}});
    value.controlIds.push_back("overflow");
    EXPECT_FALSE(ValidateSoftBodyAsset(value));
}
