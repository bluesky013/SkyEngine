//
// Created on 2026/10/07.
//

#include <core/environment/Once.h>
#include <core/type/TypeInfo.h>
#include <editor/core/property/ReflectedForm.h>
#include <framework/serialization/SerializationContext.h>

#include <gtest/gtest.h>

#include "TestTypes.h"

using namespace sky::editor;

namespace {

    struct InlineFieldLeaf {
        int32_t value   = 7;
        bool    enabled = true;
    };

    struct MetadataOrderFields {
        int32_t zeta     = 7;
        int32_t middle   = 11;
        int32_t zero     = 0;
        int32_t alpha    = 3;
        int32_t fallback = 5;
        int32_t invalid  = 6;
        int32_t hidden   = 99;
    };

    struct MetadataOrderLeaf {
        int32_t first  = 7;
        int32_t second = 11;
        int32_t hidden = 99;
    };
    struct MetadataOrderContainer {
        MetadataOrderLeaf    alpha;
        MetadataOrderLeaf    beta;
        std::vector<int32_t> values{2, 4};
    };
    static_assert(sizeof(MetadataOrderLeaf) < sky::Any::BLOCK_SIZE);

    void RegisterMetadataOrderTypes()
    {
        if (!sky::Once::Get()->Run("ReflectedFormTest.MetadataOrderTypes"))
            return;
        auto *context = sky::SerializationContext::Get();
        context->Register<MetadataOrderFields>("ReflectedMetadataOrderFields")
            .Member<&MetadataOrderFields::zeta>("zeta")
            .Property(sky::CommonPropertyKey::ORDER, sky::Any(int32_t{-5}))
            .Member<&MetadataOrderFields::middle>("middle")
            .Property(sky::CommonPropertyKey::ORDER, sky::Any(int32_t{5}))
            .Member<&MetadataOrderFields::zero>("zero")
            .Property(sky::CommonPropertyKey::ORDER, sky::Any(int32_t{0}))
            .Member<&MetadataOrderFields::alpha>("alpha")
            .Property(sky::CommonPropertyKey::ORDER, sky::Any(int32_t{5}))
            .Member<&MetadataOrderFields::invalid>("invalid")
            .Property(sky::CommonPropertyKey::ORDER, sky::Any(std::string("invalid")))
            .Member<&MetadataOrderFields::fallback>("fallback")
            .Member<&MetadataOrderFields::hidden>("hidden")
            .Property(sky::CommonPropertyKey::ORDER, sky::Any(int32_t{-20}))
            .Property(sky::CommonPropertyKey::VISIBLE, sky::Any(false));
        context->Register<MetadataOrderLeaf>("ReflectedMetadataOrderLeaf")
            .Member<&MetadataOrderLeaf::first>("first")
            .Property(sky::CommonPropertyKey::ORDER, sky::Any(int32_t{5}))
            .Member<&MetadataOrderLeaf::hidden>("hidden")
            .Property(sky::CommonPropertyKey::VISIBLE, sky::Any(false))
            .Member<&MetadataOrderLeaf::second>("second")
            .Property(sky::CommonPropertyKey::ORDER, sky::Any(int32_t{-5}));
        context->Register<MetadataOrderContainer>("ReflectedMetadataOrderContainer")
            .Member<&MetadataOrderContainer::alpha>("alpha")
            .Property(sky::CommonPropertyKey::ORDER, sky::Any(int32_t{5}))
            .Member<&MetadataOrderContainer::beta>("beta")
            .Property(sky::CommonPropertyKey::ORDER, sky::Any(int32_t{-5}))
            .Member<&MetadataOrderContainer::values>("values")
            .Property(sky::CommonPropertyKey::ORDER, sky::Any(int32_t{-10}));
    }

    std::vector<std::string> FieldPaths(const std::vector<PropertyField> &fields)
    {
        std::vector<std::string> result;
        for (const auto &field : fields)
            result.push_back(field.path);
        return result;
    }

    struct InlineFieldBranch {
        InlineFieldLeaf left;
        InlineFieldLeaf right;
    };
    static_assert(sizeof(InlineFieldLeaf) < sky::Any::BLOCK_SIZE);
    static_assert(sizeof(InlineFieldBranch) < sky::Any::BLOCK_SIZE);

    struct FieldSequenceBranch {
        std::vector<int32_t> items{2, 4, 6};
        InlineFieldBranch    nested;
    };

    struct FieldSequenceHolder {
        FieldSequenceBranch deeper;
        InlineFieldLeaf     sibling;
    };

    struct FieldStorageObject {
        InlineFieldBranch   alpha;
        InlineFieldBranch   beta;
        InlineFieldBranch   gamma;
        InlineFieldBranch   delta;
        InlineFieldBranch   epsilon;
        InlineFieldBranch   zeta;
        FieldSequenceHolder sequences;
    };

    const sky::TypeNode *StorageType()
    {
        if (sky::Once::Get()->Run("EditorCoreTest.ReflectedFieldStorageTypes")) {
            auto *context = sky::SerializationContext::Get();
            context->Register<InlineFieldLeaf>("ReflectedFieldInlineLeaf")
                .Member<&InlineFieldLeaf::value>("value")
                .Member<&InlineFieldLeaf::enabled>("enabled");
            context->Register<InlineFieldBranch>("ReflectedFieldInlineBranch")
                .Member<&InlineFieldBranch::left>("left")
                .Member<&InlineFieldBranch::right>("right");
            context->Register<FieldSequenceBranch>("ReflectedFieldSequenceBranch")
                .Member<&FieldSequenceBranch::items>("items")
                .Member<&FieldSequenceBranch::nested>("nested");
            context->Register<FieldSequenceHolder>("ReflectedFieldSequenceHolder")
                .Member<&FieldSequenceHolder::deeper>("deeper")
                .Member<&FieldSequenceHolder::sibling>("sibling");
            context->Register<FieldStorageObject>("ReflectedFieldStorageObject")
                .Member<&FieldStorageObject::alpha>("alpha")
                .Member<&FieldStorageObject::beta>("beta")
                .Member<&FieldStorageObject::gamma>("gamma")
                .Member<&FieldStorageObject::delta>("delta")
                .Member<&FieldStorageObject::epsilon>("epsilon")
                .Member<&FieldStorageObject::zeta>("zeta")
                .Member<&FieldStorageObject::sequences>("sequences");
        }
        return sky::GetTypeNode(sky::TypeInfo<FieldStorageObject>::RegisteredId());
    }

    PropertyField *FindNestedField(PropertyField &field, const std::string &path)
    {
        if (field.path == path) {
            return &field;
        }
        for (auto &child : field.children) {
            if (auto *found = FindNestedField(child, path)) {
                return found;
            }
        }
        return nullptr;
    }

    PropertyField *FindNestedField(ReflectedForm &form, const std::string &path)
    {
        for (auto &section : form.GetSections()) {
            for (auto &field : section.fields) {
                if (auto *found = FindNestedField(field, path)) {
                    return found;
                }
            }
        }
        return nullptr;
    }

    void CheckFieldParents(PropertyField &field, PropertyField *parent = nullptr)
    {
        EXPECT_EQ(field.parent, parent) << field.path;
        for (auto &child : field.children) {
            CheckFieldParents(child, &field);
        }
    }

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

TEST(ReflectedFormTest, InlineNestedStorageSurvivesSiblingOrderingAndDefaultCapture)
{
    const auto            *type = StorageType();
    FieldStorageObject     object;
    InlineFieldBranch     *branches[] = {&object.alpha, &object.beta, &object.gamma, &object.delta, &object.epsilon, &object.zeta};
    const char            *names[]    = {"alpha", "beta", "gamma", "delta", "epsilon", "zeta"};
    PropertyEditorRegistry registry;
    for (size_t i = 0; i < 6; ++i) {
        branches[i]->left.value  = 100 + static_cast<int32_t>(i);
        branches[i]->right.value = 200 + static_cast<int32_t>(i);
        registry.RegisterMemberAppearance(type->info->registeredId, names[i], {names[i], 6 - static_cast<int>(i), true});
    }
    registry.RegisterMemberAppearance(sky::TypeInfo<InlineFieldBranch>::RegisteredId(), "right", {"Right", -1, true});
    registry.RegisterMemberAppearance(sky::TypeInfo<InlineFieldLeaf>::RegisteredId(), "value", {"Value", -1, true});
    ReflectedForm form;
    form.Build(PropertyObject{&object, type}, registry);
    ASSERT_EQ(form.GetSections().size(), 1u);
    auto &fields = form.GetSections().front().fields;
    ASSERT_EQ(fields.size(), 7u); // Former push_back growth moved inline backing repeatedly.
    EXPECT_EQ(fields.front().path, "sequences");
    for (auto &field : fields) {
        CheckFieldParents(field);
    }
    for (size_t i = 0; i < 6; ++i) {
        PropertyField *branch = FindNestedField(form, names[i]);
        ASSERT_NE(branch, nullptr);
        ASSERT_EQ(branch->children.size(), 2u);
        EXPECT_EQ(branch->children.front().path, std::string(names[i]) + ".right");
        PropertyField *left  = FindNestedField(form, std::string(names[i]) + ".left.value");
        PropertyField *right = FindNestedField(form, std::string(names[i]) + ".right.value");
        ASSERT_NE(left, nullptr);
        ASSERT_NE(right, nullptr);
        const sky::Any leftValue  = left->descriptor.GetValue();
        const sky::Any rightValue = right->descriptor.GetValue();
        ASSERT_NE(leftValue.GetAsConst<int32_t>(), nullptr);
        ASSERT_NE(rightValue.GetAsConst<int32_t>(), nullptr);
        EXPECT_EQ(*leftValue.GetAsConst<int32_t>(), 100 + static_cast<int32_t>(i));
        EXPECT_EQ(*rightValue.GetAsConst<int32_t>(), 200 + static_cast<int32_t>(i));
        ASSERT_NE(left->defaultValue.GetAsConst<int32_t>(), nullptr);
        EXPECT_EQ(*left->defaultValue.GetAsConst<int32_t>(), 7);
        EXPECT_TRUE(form.IsModified(*left));
    }
}

TEST(ReflectedFormTest, InlineNestedEditAndResetPropagateThroughStableParents)
{
    FieldStorageObject object;
    object.alpha.left.value  = 41;
    object.alpha.right.value = 73;
    PropertyEditorRegistry registry;
    ReflectedForm          form;
    form.Build(PropertyObject{&object, StorageType()}, registry);
    CommandService commands;
    PropertyField *value = FindNestedField(form, "alpha.left.value");
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(form.Edit(*value, sky::Any(int32_t{99}), commands));
    EXPECT_EQ(object.alpha.left.value, 99);
    EXPECT_EQ(object.alpha.right.value, 73);
    EXPECT_EQ(object.beta.left.value, 7);
    ASSERT_TRUE(commands.Undo());
    EXPECT_EQ(object.alpha.left.value, 41);
    form.Rebuild();
    value = FindNestedField(form, "alpha.left.value");
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(form.ResetToDefault(*value, commands));
    EXPECT_EQ(object.alpha.left.value, 7);
    EXPECT_EQ(object.alpha.right.value, 73);
    ASSERT_TRUE(commands.Undo());
    EXPECT_EQ(object.alpha.left.value, 41);
}

TEST(ReflectedFormTest, MetadataOrderUsesStableReflectionTiesAndDistinguishesExplicitZero)
{
    RegisterMetadataOrderTypes();
    MetadataOrderFields    object;
    PropertyEditorRegistry registry;
    ReflectedForm          form;
    const auto            *type = sky::GetTypeNode(sky::TypeInfo<MetadataOrderFields>::RegisteredId());
    form.Build({&object, type}, registry);
    EXPECT_EQ(FieldPaths(form.GetSections().front().fields), (std::vector<std::string>{"zeta", "fallback", "invalid", "zero", "alpha", "middle"}));
    // Registration deliberately puts middle before alpha and invalid before
    // fallback; equal orders preserve MemberMap iteration, not those calls.
    EXPECT_TRUE(FindField(form, "zero")->hasOrder);
    EXPECT_EQ(FindField(form, "zero")->order, 0);
    EXPECT_FALSE(FindField(form, "fallback")->hasOrder);
    EXPECT_FALSE(FindField(form, "invalid")->hasOrder);
    EXPECT_EQ(FindField(form, "invalid")->order, 0);
    EXPECT_EQ(FindField(form, "zeta")->order, -5);
    EXPECT_EQ(FindField(form, "hidden"), nullptr);
}

TEST(ReflectedFormTest, ExplicitAppearanceOrderOverridesMetadataWhileLabelOnlyKeepsIt)
{
    RegisterMetadataOrderTypes();
    MetadataOrderFields    object;
    PropertyEditorRegistry registry;
    const auto             typeId = sky::TypeInfo<MetadataOrderFields>::RegisteredId();
    registry.RegisterMemberAppearance(typeId, "alpha", {"Renamed alpha", -99, false});
    registry.RegisterMemberAppearance(typeId, "zeta", {"Late zeta", 99, true});
    registry.RegisterMemberAppearance(typeId, "middle", {"First middle", -10, true});
    ReflectedForm form;
    form.Build({&object, sky::GetTypeNode(typeId)}, registry);
    EXPECT_EQ(FieldPaths(form.GetSections().front().fields), (std::vector<std::string>{"middle", "fallback", "invalid", "zero", "alpha", "zeta"}));
    EXPECT_EQ(FindField(form, "alpha")->label, "Renamed alpha");
    EXPECT_EQ(FindField(form, "alpha")->order, 5);
    EXPECT_TRUE(FindField(form, "alpha")->hasOrder);
    EXPECT_EQ(FindField(form, "middle")->order, -10);
    EXPECT_EQ(FindField(form, "zeta")->order, 99);
}

TEST(ReflectedFormTest, MetadataOrderingKeepsNestedStorageHiddenFieldsAndSequenceEditsSafe)
{
    RegisterMetadataOrderTypes();
    MetadataOrderContainer object;
    object.beta.first  = 83;
    object.beta.second = 93;
    PropertyEditorRegistry registry;
    ReflectedForm          form;
    form.Build({&object, sky::GetTypeNode(sky::TypeInfo<MetadataOrderContainer>::RegisteredId())}, registry);
    EXPECT_EQ(FieldPaths(form.GetSections().front().fields), (std::vector<std::string>{"values", "beta", "alpha"}));
    auto *beta = FindField(form, "beta");
    ASSERT_NE(beta, nullptr);
    EXPECT_EQ(FieldPaths(beta->children), (std::vector<std::string>{"beta.second", "beta.first"}));
    EXPECT_EQ(FindNestedField(form, "beta.hidden"), nullptr);
    for (auto &field : form.GetSections().front().fields)
        CheckFieldParents(field);
    auto *first = FindNestedField(form, "beta.first");
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(*first->descriptor.GetValue().GetAsConst<int32_t>(), 83);
    EXPECT_EQ(first->order, 5);
    EXPECT_TRUE(first->hasOrder);
    CommandService commands;
    ASSERT_TRUE(form.ResetToDefault(*first, commands));
    EXPECT_EQ(object.beta.first, 7);
    EXPECT_EQ(object.beta.second, 93);
    ASSERT_TRUE(commands.Undo());
    EXPECT_EQ(object.beta.first, 83);
    form.Rebuild();
    auto *values = FindField(form, "values");
    ASSERT_NE(values, nullptr);
    ASSERT_EQ(values->children.size(), 2u);
    EXPECT_EQ(values->children.front().path, "values[0]");
    EXPECT_FALSE(values->children.front().hasOrder);
    EXPECT_EQ(*values->children.front().descriptor.GetValue().GetAsConst<int32_t>(), 2);
    ASSERT_TRUE(form.AddSequenceElement(*values, commands));
    EXPECT_EQ(object.values.size(), 3u);
    ASSERT_TRUE(commands.Undo());
    EXPECT_EQ(object.values, (std::vector<int32_t>{2, 4}));
    form.Rebuild();
    first = FindNestedField(form, "beta.first");
    ASSERT_NE(first, nullptr);
    ASSERT_TRUE(form.Edit(*first, sky::Any(int32_t{101}), commands));
    EXPECT_EQ(object.beta.first, 101);
    EXPECT_EQ(object.alpha.first, 7);
    ASSERT_TRUE(commands.Undo());
    EXPECT_EQ(object.beta.first, 83);
}

TEST(ReflectedFormTest, MultilevelSequenceDescriptorsAndDefaultsSurviveFinalStorageBuild)
{
    FieldStorageObject object;
    for (int32_t i = 0; i < 20; ++i) {
        object.sequences.deeper.items.push_back(100 + i);
    }
    object.sequences.deeper.nested.right.value = 81;
    FieldStorageObject baseline;
    baseline.sequences.deeper.nested.right.value = 17;
    PropertyEditorRegistry registry;
    ReflectedForm          form;
    const PropertyObject   baselineObject{&baseline, StorageType()};
    form.Build(PropertyObject{&object, StorageType()}, registry, &baselineObject);
    PropertyField *sequence = FindNestedField(form, "sequences.deeper.items");
    ASSERT_NE(sequence, nullptr);
    ASSERT_TRUE(sequence->isSequence);
    ASSERT_EQ(sequence->children.size(), 23u);
    for (size_t i = 0; i < sequence->children.size(); ++i) {
        const auto &child = sequence->children[i];
        EXPECT_EQ(child.parent, sequence);
        EXPECT_TRUE(child.descriptor.IsSequenceElement());
        EXPECT_FALSE(child.descriptor.CanEdit());
        const sky::Any value = child.descriptor.GetValue();
        ASSERT_NE(value.GetAsConst<int32_t>(), nullptr);
        EXPECT_EQ(*value.GetAsConst<int32_t>(), object.sequences.deeper.items[i]);
    }
    PropertyField *nested = FindNestedField(form, "sequences.deeper.nested.right.value");
    ASSERT_NE(nested, nullptr);
    ASSERT_NE(nested->defaultValue.GetAsConst<int32_t>(), nullptr);
    EXPECT_EQ(*nested->defaultValue.GetAsConst<int32_t>(), 17);
    CommandService commands;
    ASSERT_TRUE(form.AddSequenceElement(*sequence, commands));
    EXPECT_EQ(object.sequences.deeper.items.size(), 24u);
    ASSERT_TRUE(commands.Undo());
    EXPECT_EQ(object.sequences.deeper.items.size(), 23u);
    form.Rebuild();
    sequence = FindNestedField(form, "sequences.deeper.items");
    ASSERT_NE(sequence, nullptr);
    ASSERT_TRUE(form.RemoveSequenceElement(*sequence, 1, commands));
    EXPECT_EQ(object.sequences.deeper.items.size(), 22u);
    ASSERT_TRUE(commands.Undo());
    EXPECT_EQ(object.sequences.deeper.items.size(), 23u);
    EXPECT_EQ(object.sequences.deeper.items[1], 4);
}
