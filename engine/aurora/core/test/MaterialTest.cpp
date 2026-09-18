//
// Material tests: inline techniques + properties + MaterialInstance partial override.
//

#include <aurora/resource/Material.h>
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
    auto shader = sky::CounterPtr<Shader>(new Shader());

    Material material;
    MaterialTechnique technique;
    technique.techniqueTag = sky::Name("opaque");
    technique.shader       = shader;
    material.AddTechnique(technique);

    ASSERT_EQ(material.GetTechniques().size(), 1u);
    const MaterialTechnique *found = material.GetTechnique(sky::Name("opaque"));
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->shader.Get(), shader.Get());
    EXPECT_EQ(material.GetTechnique(sky::Name("missing")), nullptr);
}

TEST(MaterialTest, Properties)
{
    Material material;

    const sky::Vector4 white(1.f, 1.f, 1.f, 1.f);
    material.AddValue(sky::Name("baseColor"), sizeof(sky::Vector4), &white);

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
    const float          roughDefault = 0.5f;
    material->AddValue(sky::Name("baseColor"), sizeof(sky::Vector4), &white);
    material->AddValue(sky::Name("roughness"), sizeof(float), &roughDefault);

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
