//
// Created on 2026/10/07.
//

#include <core/type/TypeInfo.h>
#include <editor/core/property/ReflectedForm.h>
#include <framework/serialization/SerializationContext.h>

#include <gtest/gtest.h>

#include "TestTypes.h"

using namespace sky::editor;

namespace {

    const sky::TypeNode *TestType()
    {
        test::RegisterTestTypes();
        return sky::GetTypeNode(sky::TypeInfo<test::TestObject>::RegisteredId());
    }

    PropertyField *FindField(ReflectedForm &form, const std::string &path)
    {
        for (auto &section : form.GetSections()) {
            for (auto &field : section.fields) {
                if (field.path == path) {
                    return &field;
                }
            }
        }
        return nullptr;
    }

} // namespace

TEST(ReflectedFormTest, ResetBaselineIsTypeDefault)
{
    const sky::TypeNode *type = TestType();
    ASSERT_NE(type, nullptr);

    test::TestObject object; // default: value == 0
    object.value = 5.f;

    PropertyEditorRegistry registry;
    ReflectedForm          form;
    form.Build(PropertyObject{&object, type}, registry);

    PropertyField *value = FindField(form, "value");
    ASSERT_NE(value, nullptr);
    // Differs from the type default (0) -> reset affordance is present.
    EXPECT_TRUE(form.IsModified(*value));
}

TEST(ReflectedFormTest, ResetToDefaultWritesTypeDefault)
{
    const sky::TypeNode *type = TestType();
    ASSERT_NE(type, nullptr);

    test::TestObject object;
    object.value = 5.f;

    PropertyEditorRegistry registry;
    ReflectedForm          form;
    form.Build(PropertyObject{&object, type}, registry);

    PropertyField *value = FindField(form, "value");
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(form.IsModified(*value));

    CommandService commands;
    ASSERT_TRUE(form.ResetToDefault(*value, commands));
    EXPECT_FLOAT_EQ(object.value, 0.f); // written back to the type default
    EXPECT_FALSE(form.IsModified(*value));
}

TEST(ReflectedFormTest, ResetRestoresEachConfigField)
{
    test::RegisterTestTypes();
    const sky::TypeNode *type = sky::GetTypeNode(sky::TypeInfo<test::TestConfig>::RegisteredId());
    ASSERT_NE(type, nullptr);

    test::TestConfig object; // defaults: gravity -9.81, steps 4, enabled true
    object.gravity = 0.f;
    object.steps   = 99;
    object.enabled = false;

    PropertyEditorRegistry registry;
    ReflectedForm          form;
    form.Build(PropertyObject{&object, type}, registry);

    CommandService commands;
    for (const char *name : {"gravity", "steps", "enabled"}) {
        PropertyField *field = FindField(form, name);
        ASSERT_NE(field, nullptr) << name;
        ASSERT_TRUE(form.IsModified(*field)) << name;
        ASSERT_TRUE(form.ResetToDefault(*field, commands)) << name;
    }

    EXPECT_FLOAT_EQ(object.gravity, -9.81f);
    EXPECT_EQ(object.steps, 4);
    EXPECT_TRUE(object.enabled);
}

TEST(ReflectedFormTest, ModifiedSurvivesRebind)
{
    const sky::TypeNode *type = TestType();
    ASSERT_NE(type, nullptr);

    test::TestObject object;
    object.value = 5.f;

    PropertyEditorRegistry registry;
    ReflectedForm          form;
    form.Build(PropertyObject{&object, type}, registry);

    // Rebinding (as when the object is reloaded) keeps the type-default baseline,
    // so the reset affordance persists across load/save.
    form.Build(PropertyObject{&object, type}, registry);

    PropertyField *value = FindField(form, "value");
    ASSERT_NE(value, nullptr);
    EXPECT_TRUE(form.IsModified(*value));
}
