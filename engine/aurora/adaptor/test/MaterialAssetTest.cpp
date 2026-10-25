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
        technique.aliases                             = {{"baseColor", "gMaterial.color"}};
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
    ASSERT_EQ(out.techniques[0].aliases.size(), 1u);
    EXPECT_EQ(out.techniques[0].aliases.front().alias, "baseColor");
    EXPECT_EQ(out.techniques[0].aliases.front().target, "gMaterial.color");
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
    const char *json =
        R"({"techniques":[{"tag":"T","program":{"vertex":{"relativePath":"custom/a.slang","entry":"Vertex"},"fragment":{"relativePath":"custom/a.slang","entry":"Surface"}},"aliases":[{"alias":"Albedo","target":"gMaterial.albedo"}],"renderState":{"depthStencil":{"depthTest":true},"rasterState":{"cullMode":2}}}],"properties":[{"name":"Metallic","type":0,"value":{"index":0,"value":0.5}},{"name":"Albedo","type":3,"value":{"index":3,"value":[1.0,1.0,1.0,1.0]}}]})";
    IMemoryArchive            mem(json, std::strlen(json));
    JsonInputArchive          ar(mem);
    aurora::MaterialAssetData data;
    ar.LoadValueObject(data);
    ASSERT_EQ(data.techniques.size(), 1u);
    EXPECT_EQ(data.techniques[0].program.fragment.entry, "Surface");
    ASSERT_EQ(data.techniques[0].aliases.size(), 1u);
    ASSERT_EQ(data.properties.size(), 2u);
    EXPECT_EQ(std::get<float>(data.properties[0].value), 0.5f);
    EXPECT_TRUE(std::holds_alternative<Vector4>(data.properties[1].value));
}

TEST(MaterialAssetTest, MalformedTechniqueCannotSilentlyUseDefaults)
{
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

TEST(MaterialAssetTest, TypedSamplerAndAliasesRoundTripAndRejectSemanticConflicts)
{
    auto                        in = MakeMaterialData();
    aurora::Sampler::Descriptor sampler;
    sampler.minFilter    = aurora::Filter::NEAREST;
    sampler.addressModeU = aurora::WrapMode::CLAMP_TO_EDGE;
    in.properties.push_back({"filter", aurora::MaterialPropertyType::SAMPLER, sampler});
    in.techniques.front().aliases.push_back({"filter", "gImageSampler"});
    aurora::MaterialAssetData out;
    RoundTripMaterial(in, out);
    std::string error;
    ASSERT_TRUE(aurora::ValidateMaterialAssetData(out, error)) << error;
    ASSERT_TRUE(std::holds_alternative<aurora::Sampler::Descriptor>(out.properties.back().value));
    const auto &actual = std::get<aurora::Sampler::Descriptor>(out.properties.back().value);
    EXPECT_EQ(actual.minFilter, aurora::Filter::NEAREST);
    EXPECT_EQ(actual.addressModeU, aurora::WrapMode::CLAMP_TO_EDGE);
    EXPECT_EQ(out.techniques.front().aliases.back().alias, "filter");
    EXPECT_EQ(out.techniques.front().aliases.back().target, "gImageSampler");

    auto wrong                     = out;
    wrong.properties.front().value = uint32_t{7};
    EXPECT_FALSE(aurora::ValidateMaterialAssetData(wrong, error));
    wrong = out;
    wrong.techniques.front().aliases.push_back({"albedo", "gImageSampler"});
    EXPECT_FALSE(aurora::ValidateMaterialAssetData(wrong, error));
    wrong = out;
    wrong.techniques.front().aliases.push_back({"missing", "gUnused"});
    EXPECT_FALSE(aurora::ValidateMaterialAssetData(wrong, error));
}

TEST(MaterialAssetTest, SamplerReflectionSupportsGenericTypedEditsAndKeepsAliases)
{
    const auto *node = GetTypeNode(TypeInfo<aurora::Sampler::Descriptor>::RegisteredId());
    ASSERT_NE(node, nullptr);
    ASSERT_EQ(node->members.size(), 10u);
    for (const auto &[name, member] : node->members) {
        SCOPED_TRACE(name);
        EXPECT_NE(member.setterFn, nullptr);
        EXPECT_NE(member.getterConstFn, nullptr);
    }
    const auto *filterType = GetTypeNode(TypeInfo<aurora::Filter>::RegisteredId());
    const auto *mipType    = GetTypeNode(TypeInfo<aurora::MipFilter>::RegisteredId());
    const auto *wrapType   = GetTypeNode(TypeInfo<aurora::WrapMode>::RegisteredId());
    ASSERT_NE(filterType, nullptr);
    ASSERT_NE(mipType, nullptr);
    ASSERT_NE(wrapType, nullptr);
    EXPECT_EQ(filterType->enums.size(), 2u);
    EXPECT_EQ(mipType->enums.size(), 2u);
    EXPECT_EQ(wrapType->enums.size(), 5u);

    aurora::Sampler::Descriptor sampler;
    const auto                 *filter = GetTypeMember("minFilter", TypeInfo<aurora::Sampler::Descriptor>::RegisteredId());
    const auto                 *lod    = GetTypeMember("maxLod", TypeInfo<aurora::Sampler::Descriptor>::RegisteredId());
    ASSERT_NE(filter, nullptr);
    ASSERT_NE(lod, nullptr);
    const auto  nearest = aurora::Filter::NEAREST;
    const float maxLod  = 4.f;
    ASSERT_TRUE(filter->setterFn(&sampler, &nearest));
    ASSERT_TRUE(lod->setterFn(&sampler, &maxLod));
    EXPECT_EQ(sampler.minFilter, aurora::Filter::NEAREST);
    EXPECT_FLOAT_EQ(sampler.maxLod, maxLod);

    auto in = MakeMaterialData();
    in.properties.push_back({"filter", aurora::MaterialPropertyType::SAMPLER, sampler});
    in.techniques.front().aliases.push_back({"filter", "gImageSampler"});
    OMemoryArchive    memory;
    JsonOutputArchive writer(memory);
    writer.SaveValueObject(in);
    IMemoryArchive            source(memory.Data(), memory.Size());
    JsonInputArchive          reader(source);
    aurora::MaterialAssetData out;
    reader.LoadValueObject(out);
    ASSERT_EQ(out.techniques.size(), 1u);
    ASSERT_EQ(out.properties.size(), in.properties.size());
    ASSERT_EQ(out.techniques.front().aliases.size(), 2u);
    EXPECT_EQ(out.techniques.front().aliases.back().target, "gImageSampler");
    ASSERT_TRUE(std::holds_alternative<aurora::Sampler::Descriptor>(out.properties.back().value));
    const auto &actual = std::get<aurora::Sampler::Descriptor>(out.properties.back().value);
    EXPECT_EQ(actual.minFilter, aurora::Filter::NEAREST);
    EXPECT_FLOAT_EQ(actual.maxLod, maxLod);
}

TEST(MaterialAssetData, TypedOptionsRoundTripThroughJsonAndBinary)
{
    auto source                                  = MakeMaterialData();
    source.techniques[0].program.variant.entries = {{Name("SIGNED"), uint32_t(-3), aurora::ShaderVariantType::INT32},
                                                    {Name("ENABLED"), 1, aurora::ShaderVariantType::BOOL}};
    aurora::MaterialAssetData binary;
    RoundTripMaterial(source, binary);
    EXPECT_EQ(binary.techniques[0].program.variant.entries, source.techniques[0].program.variant.entries);
    OMemoryArchive    memory;
    JsonOutputArchive writer(memory);
    writer.SaveValueObject(source);
    IMemoryArchive            input(memory.Data(), memory.Size());
    JsonInputArchive          reader(input);
    aurora::MaterialAssetData json;
    reader.LoadValueObject(json);
    EXPECT_EQ(json.techniques[0].program.variant.entries, source.techniques[0].program.variant.entries);
}

TEST(MaterialAssetData, GeneratedPropertyReflectionEditsOnlyTheSelectedTypedValue)
{
    aurora::MaterialPropertyData property{"TEXTURED", aurora::MaterialPropertyType::BOOL, false};
    auto                         node = aurora::MakeMaterialPropertyReflection(property);
    ASSERT_TRUE(node);
    ASSERT_EQ(node->members.size(), 1u);
    const auto &member  = node->members.begin()->second;
    const bool  enabled = true;
    ASSERT_TRUE(member.setterFn(&property, &enabled));
    EXPECT_TRUE(std::get<bool>(property.value));
    const auto value = member.getterConstFn(&property);
    ASSERT_TRUE(value.GetAsConst<bool>());
    EXPECT_TRUE(*value.GetAsConst<bool>());
    property.type       = aurora::MaterialPropertyType::FLOAT;
    const bool disabled = false;
    EXPECT_FALSE(member.setterFn(&property, &disabled));
    EXPECT_TRUE(std::get<bool>(property.value));
    EXPECT_FALSE(aurora::MakeMaterialPropertyReflection(property));
}

TEST(MaterialAssetData, ShaderOptionsUseDeclaredTypesAndPreserveOutputOnFailure)
{
    using namespace aurora;
    ShaderVariantSchema schema;
    schema.entries = {{.key = Name("ENABLED"), .bitWidth = 1, .type = ShaderVariantType::BOOL},
                      {.key = Name("SIGNED"), .bitWidth = 32, .type = ShaderVariantType::INT32},
                      {.key = Name("COUNT"), .bitWidth = 2},
                      {.key = Name("SHADOWS"), .bitWidth = 1, .domain = ShaderVariantDomain::PIPELINE}};
    std::vector<MaterialPropertyData> properties{{"ENABLED", MaterialPropertyType::BOOL, true},
                                                 {"SIGNED", MaterialPropertyType::INT, int32_t{-3}},
                                                 {"COUNT", MaterialPropertyType::UINT, uint32_t{2}},
                                                 {"SHADOWS", MaterialPropertyType::UINT, uint32_t{1}},
                                                 {"gMaterial.tint", MaterialPropertyType::FLOAT4, Vector4{}}};
    ShaderVariant                     values;
    std::string                       error;
    ASSERT_TRUE(AppendMaterialShaderOptions(schema, properties, values, error)) << error;
    ASSERT_EQ(values.entries.size(), 3u);
    EXPECT_EQ(values.entries[0], (ShaderVariantEntry{Name("ENABLED"), 1, ShaderVariantType::BOOL}));
    EXPECT_EQ(values.entries[1], (ShaderVariantEntry{Name("SIGNED"), uint32_t(-3), ShaderVariantType::INT32}));
    EXPECT_EQ(values.entries[2], (ShaderVariantEntry{Name("COUNT"), 2, ShaderVariantType::UINT32}));
    const auto original = values.entries;
    properties[2].value = uint32_t{4};
    EXPECT_FALSE(AppendMaterialShaderOptions(schema, properties, values, error));
    EXPECT_EQ(values.entries, original);
    properties[2] = {"COUNT", MaterialPropertyType::INT, int32_t{2}};
    EXPECT_FALSE(AppendMaterialShaderOptions(schema, properties, values, error));
    EXPECT_EQ(values.entries, original);
    properties[2] = {"COUNT", MaterialPropertyType::FLOAT, 2.f};
    EXPECT_FALSE(AppendMaterialShaderOptions(schema, properties, values, error));
    EXPECT_EQ(values.entries, original);
}
