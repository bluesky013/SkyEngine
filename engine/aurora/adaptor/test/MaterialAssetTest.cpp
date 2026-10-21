//
// MaterialAssetData / MaterialInstanceAssetData round-trip and version guard.
//

#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MaterialInstanceAsset.h>
#include <aurora/adaptor/assets/MaterialReflection.h>

#include <core/archive/MemoryStreamArchive.h>
#include <framework/serialization/BinaryArchive.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/SerializationContext.h>

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
        technique.program.vertex                      = aurora::ShaderRef{"shaders/opaque.slang", "main"};
        technique.program.fragment                    = {"custom/Shader.slang", "SurfaceCustom"};
        technique.program.variant.entries             = {{Name("MODE"), 1}};
        technique.uniforms                            = {{3, {{"baseColor", "color"}}}};
        technique.renderState.alphaToCoverage         = true;
        technique.renderState.depthStencil.depthTest  = true;
        technique.renderState.depthStencil.depthWrite = true;
        technique.renderState.rasterState.cullMode    = aurora::CullModeFlagBits::BACK;
        technique.renderState.blendStates.push_back(aurora::BlendState{});
        data.techniques.push_back(technique);

        aurora::MaterialPropertyData color;
        color.name = "baseColor";
        color.type = aurora::MaterialPropertyType::FLOAT4;
        sky::Vector4 rgba{};
        rgba.v[0]   = 1.f;
        rgba.v[1]   = 0.f;
        rgba.v[2]   = 0.f;
        rgba.v[3]   = 1.f;
        color.value = rgba;
        data.properties.push_back(color);

        aurora::MaterialPropertyData flag;
        flag.name  = "useNormalMap";
        flag.type  = aurora::MaterialPropertyType::BOOL;
        flag.value = true;
        data.properties.push_back(flag);

        aurora::MaterialPropertyData albedo;
        albedo.name  = "albedo";
        albedo.type  = aurora::MaterialPropertyType::TEXTURE2D;
        albedo.value = Uuid::CreateWithSeed(7);
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
    EXPECT_EQ(out.techniques[0].program.vertex.relativePath, "shaders/opaque.slang");
    EXPECT_EQ(out.techniques[0].program.vertex.entry, "main");
    EXPECT_EQ(out.techniques[0].program.fragment.entry, "SurfaceCustom");
    ASSERT_EQ(out.techniques[0].program.variant.entries.size(), 1u);
    EXPECT_EQ(out.techniques[0].program.variant.entries.front().value, 1u);
    ASSERT_EQ(out.techniques[0].uniforms.size(), 1u);
    EXPECT_EQ(out.techniques[0].uniforms.front().fields.front().property, "baseColor");
    EXPECT_EQ(out.techniques[0].uniforms.front().binding, 3u);
    EXPECT_TRUE(out.techniques[0].renderState.alphaToCoverage);
    EXPECT_TRUE(out.techniques[0].renderState.depthStencil.depthTest);
    EXPECT_EQ(out.techniques[0].renderState.blendStates.size(), 1u);

    ASSERT_EQ(out.properties.size(), in.properties.size());
    EXPECT_EQ(out.properties[0].name, "baseColor");
    EXPECT_EQ(out.properties[0].type, aurora::MaterialPropertyType::FLOAT4);
    EXPECT_TRUE(std::holds_alternative<sky::Vector4>(out.properties[0].value));
    EXPECT_EQ(std::get<Uuid>(out.properties[2].value), std::get<Uuid>(in.properties[2].value));
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
    value.name  = "roughness";
    value.type  = aurora::MaterialPropertyType::FLOAT;
    value.value = 0.25f;
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
    EXPECT_EQ(std::get<float>(out.valueOverrides[0].value), std::get<float>(in.valueOverrides[0].value));
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

TEST(MaterialAssetTest, SourceJsonCodec)
{
    aurora::RegisterMaterialReflection(SerializationContext::Get());
    const char *json =
        R"({"techniques":[{"tag":"T","program":{"vertex":{"relativePath":"custom/a.slang","entry":"Vertex"},"fragment":{"relativePath":"custom/a.slang","entry":"Surface"}},"uniforms":[{"binding":3,"fields":{"albedo":"Albedo"}}],"renderState":{"depthStencil":{"depthTest":true},"rasterState":{"cullMode":2}}}],"properties":[{"name":"Metallic","type":0,"value":{"index":0,"value":0.5}},{"name":"Albedo","type":3,"value":{"index":3,"value":[1.0,1.0,1.0,1.0]}}]})";
    IMemoryArchive            mem(json, std::strlen(json));
    JsonInputArchive          ar(mem);
    aurora::MaterialAssetData data;
    ar.LoadValueObject(data);
    ASSERT_EQ(data.techniques.size(), 1u);
    EXPECT_EQ(data.techniques[0].program.fragment.entry, "Surface");
    ASSERT_EQ(data.techniques[0].uniforms.size(), 1u);
    ASSERT_EQ(data.properties.size(), 2u);
    EXPECT_EQ(std::get<float>(data.properties[0].value), 0.5f);
    EXPECT_TRUE(std::holds_alternative<Vector4>(data.properties[1].value));
}

TEST(MaterialAssetTest, MalformedTechniqueCannotSilentlyUseDefaults)
{
    aurora::RegisterMaterialReflection(SerializationContext::Get());
    for (const char *technique :
         {R"({"tag":"opaque","shader":{"relativePath":"legacy.slang","entry":"main"}})", R"({"tag":"opaque","program":{"variant":{"MODE":"wrong"}}})",
          R"({"tag":"opaque","program":{},"uniforms":[{"binding":2,"fields":{"tint":3}}]})"}) {
        const std::string json = std::string("{\"techniques\":[") + technique + "]}";
        IMemoryArchive    mem(json.data(), json.size());
        JsonInputArchive  ar(mem);
        auto              data = MakeMaterialData();
        ar.LoadValueObject(data);
        EXPECT_EQ(data.version, 0u);
        EXPECT_TRUE(data.techniques.empty());
        EXPECT_TRUE(data.properties.empty());
    }
}
