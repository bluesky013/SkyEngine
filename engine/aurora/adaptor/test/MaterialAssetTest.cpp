//
// MaterialAssetData / MaterialInstanceAssetData round-trip and version guard.
//

#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MaterialInstanceAsset.h>

#include <core/archive/MemoryStreamArchive.h>
#include <framework/serialization/BinaryArchive.h>

#include <cstring>

#include <gtest/gtest.h>

using namespace sky;

namespace {

    void RoundTripMaterial(const aurora::MaterialAssetData &in, aurora::MaterialAssetData &out)
    {
        OMemoryArchive      outArchive;
        BinaryOutputArchive writer(outArchive);
        in.Save(writer);

        IMemoryArchive     inArchive(outArchive.Data(), outArchive.Size());
        BinaryInputArchive reader(inArchive);
        out.Load(reader);
    }

    void RoundTripInstance(const aurora::MaterialInstanceAssetData &in, aurora::MaterialInstanceAssetData &out)
    {
        OMemoryArchive      outArchive;
        BinaryOutputArchive writer(outArchive);
        in.Save(writer);

        IMemoryArchive     inArchive(outArchive.Data(), outArchive.Size());
        BinaryInputArchive reader(inArchive);
        out.Load(reader);
    }

    aurora::MaterialAssetData MakeMaterialData()
    {
        aurora::MaterialAssetData data;

        aurora::MaterialTechniqueData technique;
        technique.tag                                 = "opaque";
        technique.shader                              = aurora::ShaderRef{"shaders/opaque.slang", "main"};
        technique.renderState.alphaToCoverage         = true;
        technique.renderState.depthStencil.depthTest  = true;
        technique.renderState.depthStencil.depthWrite = true;
        technique.renderState.rasterState.cullMode    = aurora::CullModeFlagBits::BACK;
        technique.renderState.blendStates.push_back(aurora::BlendState{});
        data.techniques.push_back(technique);

        aurora::MaterialPropertyData color;
        color.name = "baseColor";
        color.type = aurora::MaterialPropertyType::FLOAT4;
        color.value.resize(sizeof(float) * 4);
        const float rgba[4] = {1.f, 0.f, 0.f, 1.f};
        std::memcpy(color.value.data(), rgba, sizeof(rgba));
        data.properties.push_back(color);

        aurora::MaterialPropertyData flag;
        flag.name  = "useNormalMap";
        flag.type  = aurora::MaterialPropertyType::BOOL;
        flag.value = {1};
        data.properties.push_back(flag);

        aurora::MaterialPropertyData albedo;
        albedo.name    = "albedo";
        albedo.type    = aurora::MaterialPropertyType::TEXTURE2D;
        albedo.texture = Uuid::CreateWithSeed(7);
        data.properties.push_back(albedo);

        return data;
    }

} // namespace

TEST(MaterialAssetTest, MaterialRoundTrip)
{
    const auto in = MakeMaterialData();

    aurora::MaterialAssetData out;
    RoundTripMaterial(in, out);

    EXPECT_EQ(out.version, aurora::MaterialAssetData::CURRENT_VERSION);
    ASSERT_EQ(out.techniques.size(), in.techniques.size());
    EXPECT_EQ(out.techniques[0].tag, "opaque");
    EXPECT_EQ(out.techniques[0].shader.relativePath, "shaders/opaque.slang");
    EXPECT_EQ(out.techniques[0].shader.entry, "main");
    EXPECT_TRUE(out.techniques[0].renderState.alphaToCoverage);
    EXPECT_TRUE(out.techniques[0].renderState.depthStencil.depthTest);
    EXPECT_EQ(out.techniques[0].renderState.blendStates.size(), 1u);

    ASSERT_EQ(out.properties.size(), in.properties.size());
    EXPECT_EQ(out.properties[0].name, "baseColor");
    EXPECT_EQ(out.properties[0].type, aurora::MaterialPropertyType::FLOAT4);
    EXPECT_EQ(out.properties[0].value, in.properties[0].value);
    EXPECT_EQ(out.properties[2].texture, in.properties[2].texture);
}

TEST(MaterialAssetTest, MaterialVersionMismatchRejected)
{
    auto in    = MakeMaterialData();
    in.version = aurora::MaterialAssetData::CURRENT_VERSION + 1;

    aurora::MaterialAssetData out;
    RoundTripMaterial(in, out);

    EXPECT_TRUE(out.techniques.empty());
    EXPECT_TRUE(out.properties.empty());
}

TEST(MaterialAssetTest, InstanceRoundTrip)
{
    aurora::MaterialInstanceAssetData in;
    in.material = Uuid::CreateWithSeed(3);

    aurora::MaterialInstanceValueOverride value;
    value.name = "roughness";
    value.type = aurora::MaterialPropertyType::FLOAT;
    value.value.resize(sizeof(float));
    const float rough = 0.25f;
    std::memcpy(value.value.data(), &rough, sizeof(rough));
    in.valueOverrides.push_back(value);

    aurora::MaterialInstanceTextureOverride texture;
    texture.name    = "albedo";
    texture.texture = Uuid::CreateWithSeed(9);
    in.textureOverrides.push_back(texture);

    aurora::MaterialInstanceAssetData out;
    RoundTripInstance(in, out);

    EXPECT_EQ(out.version, aurora::MaterialInstanceAssetData::CURRENT_VERSION);
    EXPECT_EQ(out.material, in.material);
    ASSERT_EQ(out.valueOverrides.size(), 1u);
    EXPECT_EQ(out.valueOverrides[0].name, "roughness");
    EXPECT_EQ(out.valueOverrides[0].value, in.valueOverrides[0].value);
    ASSERT_EQ(out.textureOverrides.size(), 1u);
    EXPECT_EQ(out.textureOverrides[0].texture, in.textureOverrides[0].texture);
}

TEST(MaterialAssetTest, InstanceVersionMismatchRejected)
{
    aurora::MaterialInstanceAssetData in;
    in.material = Uuid::CreateWithSeed(3);
    in.version  = aurora::MaterialInstanceAssetData::CURRENT_VERSION + 1;

    aurora::MaterialInstanceAssetData out;
    RoundTripInstance(in, out);

    EXPECT_FALSE(static_cast<bool>(out.material));
    EXPECT_TRUE(out.valueOverrides.empty());
    EXPECT_TRUE(out.textureOverrides.empty());
}
