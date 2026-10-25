//
// Material tests: inline techniques + properties + MaterialInstance partial override.
//

#include <aurora/resource/Material.h>
#include <aurora/resource/MaterialSchema.h>
#include <aurora/resource/Texture.h>
#include <core/math/Vector4.h>

#include <gtest/gtest.h>

using namespace sky::aurora;

namespace {

    bool Vec4Eq(const sky::Vector4 &a, const sky::Vector4 &b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
    }

} // namespace

TEST(MaterialTest, Technique)
{
    Material          material;
    MaterialTechnique technique;
    technique.techniqueTag                = sky::Name("opaque");
    technique.program.vertex              = ShaderRef{"shaders/opaque.slang", "main"};
    technique.renderState.alphaToCoverage = true;
    technique.renderState.blendStates.push_back(BlendState{});
    material.AddTechnique(technique);

    ASSERT_EQ(material.GetTechniques().size(), 1u);
    const MaterialTechnique *found = material.GetTechnique(sky::Name("opaque"));
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->program.vertex.relativePath, "shaders/opaque.slang");
    EXPECT_EQ(found->program.vertex.entry, "main");
    EXPECT_TRUE(found->renderState.alphaToCoverage);
    EXPECT_EQ(found->renderState.blendStates.size(), 1u);
    EXPECT_EQ(material.GetTechnique(sky::Name("missing")), nullptr);
}

TEST(MaterialTest, Properties)
{
    Material material;

    const sky::Vector4 white(1.f, 1.f, 1.f, 1.f);
    material.AddValue(sky::Name("baseColor"), MaterialPropertyType::FLOAT4, &white);

    sky::Vector4 read;
    ASSERT_TRUE(material.GetValue(sky::Name("baseColor"), read));
    EXPECT_TRUE(Vec4Eq(read, white));

    const sky::Vector4 red(1.f, 0.f, 0.f, 1.f);
    material.SetValue(sky::Name("baseColor"), red);
    ASSERT_TRUE(material.GetValue(sky::Name("baseColor"), read));
    EXPECT_TRUE(Vec4Eq(read, red));

    // size mismatch -> no write
    material.SetValue(sky::Name("baseColor"), 1.5f);
    ASSERT_TRUE(material.GetValue(sky::Name("baseColor"), read));
    EXPECT_TRUE(Vec4Eq(read, red));

    // undeclared key
    float missing = 0.f;
    EXPECT_FALSE(material.GetValue(sky::Name("missing"), missing));

    EXPECT_EQ(material.GetPropertyMap().size(), 1u);
}

TEST(MaterialTest, Textures)
{
    Material material;
    material.AddTexture(sky::Name("albedo"));

    auto tex = sky::CounterPtr<Texture>(new Texture2D());
    material.SetTexture(sky::Name("albedo"), tex);

    EXPECT_EQ(material.GetTexture(sky::Name("albedo")), tex.Get());
    EXPECT_EQ(material.GetTexture(sky::Name("missing")), nullptr);
}

TEST(MaterialTest, InstanceOverrideAndInherit)
{
    auto material = sky::CounterPtr<Material>(new Material());

    const sky::Vector4 white(1.f, 1.f, 1.f, 1.f);
    const float        roughDefault = 0.5f;
    material->AddValue(sky::Name("baseColor"), MaterialPropertyType::FLOAT4, &white);
    material->AddValue(sky::Name("roughness"), MaterialPropertyType::FLOAT, &roughDefault);

    MaterialInstance instance;
    instance.SetMaterial(material);
    EXPECT_EQ(instance.GetMaterial(), material.Get());

    const sky::Vector4 red(1.f, 0.f, 0.f, 1.f);
    instance.SetValue(sky::Name("baseColor"), red);

    sky::Vector4 readColor;
    ASSERT_TRUE(instance.GetValue(sky::Name("baseColor"), readColor));
    EXPECT_TRUE(Vec4Eq(readColor, red));
    EXPECT_TRUE(instance.IsOverridden(sky::Name("baseColor")));

    // not overridden -> inherits material default
    float readRough = 0.f;
    ASSERT_TRUE(instance.GetValue(sky::Name("roughness"), readRough));
    EXPECT_EQ(readRough, roughDefault);
    EXPECT_FALSE(instance.IsOverridden(sky::Name("roughness")));

    // instance override does not pollute the shared material
    sky::Vector4 matColor;
    ASSERT_TRUE(material->GetValue(sky::Name("baseColor"), matColor));
    EXPECT_TRUE(Vec4Eq(matColor, white));
}

TEST(MaterialTest, InstanceWithoutMaterial)
{
    MaterialInstance instance;

    float value = 0.f;
    instance.SetValue(sky::Name("x"), 1.0f); // no-op, must not crash
    EXPECT_FALSE(instance.GetValue(sky::Name("x"), value));
    EXPECT_EQ(instance.GetTexture(sky::Name("y")), nullptr);
    EXPECT_FALSE(instance.IsOverridden(sky::Name("x")));
}

TEST(MaterialTest, SameSizedValuesRequireTheDeclaredSemanticType)
{
    Material    material;
    const float value = .5f;
    ASSERT_TRUE(material.AddValue(sky::Name("roughness"), MaterialPropertyType::FLOAT, &value));
    EXPECT_FALSE(material.AddValue(sky::Name("badDefault"), MaterialPropertyType::FLOAT, static_cast<const uint32_t *>(nullptr)));
    EXPECT_FALSE(material.AddValue(sky::Name("roughness"), MaterialPropertyType::FLOAT, &value));
    EXPECT_FALSE(material.SetValue(sky::Name("roughness"), int32_t{7}));
    EXPECT_FALSE(material.SetValue(sky::Name("roughness"), uint32_t{7}));
    uint32_t wrongRead = 99;
    EXPECT_FALSE(material.GetValue(sky::Name("roughness"), wrongRead));
    EXPECT_EQ(wrongRead, 99u);
    float read = 0;
    ASSERT_TRUE(material.GetValue(sky::Name("roughness"), read));
    EXPECT_EQ(read, value);

    auto shared = sky::CounterPtr<Material>(new Material());
    ASSERT_TRUE(shared->AddValue(sky::Name("roughness"), MaterialPropertyType::FLOAT, &value));
    MaterialInstance instance;
    instance.SetMaterial(shared);
    EXPECT_FALSE(instance.SetValue(sky::Name("roughness"), uint32_t{9}));
    EXPECT_FALSE(instance.IsOverridden(sky::Name("roughness")));
    ASSERT_TRUE(instance.SetValue(sky::Name("roughness"), .75f));
    ASSERT_TRUE(instance.GetValue(sky::Name("roughness"), read));
    EXPECT_EQ(read, .75f);
    EXPECT_FALSE(instance.SetValue(sky::Name("roughness"), int32_t{3}));
    ASSERT_TRUE(instance.GetValue(sky::Name("roughness"), read));
    EXPECT_EQ(read, .75f);
}

TEST(MaterialTest, SamplerOverridesAreTypedAndFailurePreservesThePreviousValue)
{
    auto                material = sky::CounterPtr<Material>(new Material());
    Sampler::Descriptor initial;
    initial.minFilter = Filter::NEAREST;
    ASSERT_TRUE(material->AddSampler(sky::Name("filter"), initial));
    EXPECT_FALSE(material->SetValue(sky::Name("filter"), uint32_t{1}));
    MaterialInstance instance;
    instance.SetMaterial(material);
    Sampler::Descriptor value;
    ASSERT_TRUE(instance.GetSampler(sky::Name("filter"), value));
    EXPECT_EQ(value.minFilter, Filter::NEAREST);
    value.minFilter = Filter::LINEAR;
    ASSERT_TRUE(instance.SetSampler(sky::Name("filter"), value));
    EXPECT_TRUE(instance.IsOverridden(sky::Name("filter")));
    auto invalid         = value;
    invalid.addressModeU = static_cast<WrapMode>(UINT32_MAX);
    EXPECT_FALSE(instance.SetSampler(sky::Name("filter"), invalid));
    ASSERT_TRUE(instance.GetSampler(sky::Name("filter"), value));
    EXPECT_EQ(value.minFilter, Filter::LINEAR);
    EXPECT_EQ(value.addressModeU, WrapMode::REPEAT);
    ASSERT_TRUE(material->GetSampler(sky::Name("filter"), value));
    EXPECT_EQ(value.minFilter, Filter::NEAREST);
}

TEST(MaterialTest, TypedSnapshotsPreservePartialOverridesWhenBaseValuesChange)
{
    auto        material = sky::CounterPtr<Material>(new Material());
    const float first = .1f, second = .2f;
    ASSERT_TRUE(material->AddValue(sky::Name("first"), MaterialPropertyType::FLOAT, &first));
    ASSERT_TRUE(material->AddValue(sky::Name("second"), MaterialPropertyType::FLOAT, &second));
    MaterialInstance instance;
    instance.SetMaterial(material);
    ASSERT_TRUE(instance.SetValue(sky::Name("first"), .7f));
    ASSERT_TRUE(material->SetValue(sky::Name("second"), .8f));
    float read = 0;
    ASSERT_TRUE(instance.GetValue(sky::Name("second"), read));
    EXPECT_EQ(read, .8f);
    EXPECT_FALSE(instance.IsOverridden(sky::Name("second")));
    ShaderDataValue snapshot;
    ASSERT_TRUE(instance.GetData(sky::Name("first"), snapshot));
    EXPECT_EQ(snapshot.type, ShaderDataType::FLOAT);
    ASSERT_EQ(snapshot.bytes.size(), sizeof(float));
    std::memcpy(&read, snapshot.bytes.data(), sizeof(read));
    EXPECT_EQ(read, .7f);
    const float third = .3f;
    ASSERT_TRUE(material->AddValue(sky::Name("third"), MaterialPropertyType::FLOAT, &third));
    ASSERT_TRUE(instance.GetValue(sky::Name("third"), read));
    EXPECT_EQ(read, .3f);
    ASSERT_TRUE(instance.SetValue(sky::Name("third"), .9f));
    ASSERT_TRUE(instance.GetValue(sky::Name("first"), read));
    EXPECT_EQ(read, .7f);
    const auto previous = snapshot;
    EXPECT_FALSE(instance.GetData(sky::Name("missing"), snapshot));
    EXPECT_EQ(snapshot.type, previous.type);
    EXPECT_EQ(snapshot.bytes, previous.bytes);
}

TEST(MaterialSchema, IncludesInactiveFieldsAndOptionsWithoutOffsets)
{
    ShaderReflection reflection;
    reflection.kind           = ShaderReflectionKind::PROGRAM;
    reflection.complete       = true;
    reflection.stages         = ShaderStageFlagBit::FS;
    reflection.bindingModel   = ShaderBindingModel::VULKAN_DESCRIPTORS;
    reflection.declarations   = {{.name             = "values",
                                  .type             = ShaderResourceType::UNIFORM_BUFFER,
                                  .set              = 2,
                                  .binding          = 1,
                                  .stages           = ShaderStageFlagBit::FS,
                                  .declarationOwner = "fixture/material"}};
    reflection.declaredBlocks = {
        {"values",
         "Values",
         2,
         1,
         16,
         {{.name = "tint", .size = 16, .scalarType = ShaderScalarType::FLOAT, .kind = ShaderTypeKind::VECTOR, .cols = 4, .alignment = 16}},
         16}};
    ShaderVariantSchema options;
    options.sources         = {{sky::Name("s"), 0, 1}};
    options.entries         = {{sky::Name("TEXTURED"), sky::Name("s"), 0, 1, 1}};
    options.entries[0].type = ShaderVariantType::BOOL;
    options.totalBits       = 1;
    MaterialSchema schema;
    std::string    error;
    ASSERT_TRUE(BuildMaterialSchema(reflection, options, schema, error)) << error;
    ASSERT_EQ(schema.keys.size(), 2u);
    EXPECT_EQ(schema.keys[0].name, "TEXTURED");
    EXPECT_EQ(schema.keys[0].kind, MaterialKeyKind::OPTION);
    EXPECT_EQ(schema.keys[1].name, "values.tint");
    EXPECT_EQ(schema.keys[1].dataType, ShaderDataType::FLOAT4);
    auto incompatible             = schema;
    incompatible.keys[1].dataType = ShaderDataType::UINT;
    EXPECT_FALSE(MergeMaterialSchemas(incompatible, schema, error));
    EXPECT_EQ(schema.keys[1].dataType, ShaderDataType::FLOAT4);
    auto second    = reflection.declarations[0];
    second.name    = "second";
    second.binding = 2;
    reflection.declarations.push_back(second);
    auto block    = reflection.declaredBlocks[0];
    block.name    = "second";
    block.binding = 2;
    reflection.declaredBlocks.push_back(block);
    EXPECT_FALSE(BuildMaterialSchema(reflection, options, schema, error));
    EXPECT_EQ(schema.keys.size(), 2u);
}
