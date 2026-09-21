//
// Created on 2026/09/21.
//

#include <editor/core/property/PropertyModel.h>
#include <framework/serialization/Any.h>
#include <framework/serialization/SerializationContext.h>
#include <core/type/TypeInfo.h>
#include <gtest/gtest.h>
#include "TestTypes.h"

namespace sky::editor {
    namespace {

        const TypeNode *GetTestType()
        {
            test::RegisterTestTypes();
            return GetTypeNode(TypeInfo<test::TestObject>::RegisteredId());
        }

        const PropertyDescriptor *Find(const PropertyModel &model, const std::string &name)
        {
            for (const auto &descriptor : model.GetDescriptors()) {
                if (descriptor.GetName() == name) {
                    return &descriptor;
                }
            }
            return nullptr;
        }

    } // namespace

    TEST(PropertyModelTest, ReadWriteAndUndo)
    {
        const TypeNode *type = GetTestType();
        ASSERT_NE(type, nullptr);

        test::TestObject object;
        object.value = 1.f;

        PropertyModel model(&object, type);
        const PropertyDescriptor *value = Find(model, "value");
        ASSERT_NE(value, nullptr);

        const Any initial = value->GetValue();
        ASSERT_NE(initial.GetAsConst<float>(), nullptr);
        EXPECT_FLOAT_EQ(*initial.GetAsConst<float>(), 1.f);

        CommandService commands;
        ASSERT_TRUE(model.Edit(*value, Any(2.f), commands));
        EXPECT_FLOAT_EQ(object.value, 2.f);

        EXPECT_TRUE(commands.Undo());
        EXPECT_FLOAT_EQ(object.value, 1.f);

        EXPECT_TRUE(commands.Redo());
        EXPECT_FLOAT_EQ(object.value, 2.f);
    }

    TEST(PropertyModelTest, SequenceAddRemoveAndUndo)
    {
        const TypeNode *type = GetTestType();
        ASSERT_NE(type, nullptr);

        test::TestObject object;
        PropertyModel model(&object, type);
        const PropertyDescriptor *items = Find(model, "items");
        ASSERT_NE(items, nullptr);

        ASSERT_TRUE(items->IsSequence());
        EXPECT_EQ(items->GetSequenceCount(), 0u);

        CommandService commands;
        commands.Execute(items->MakeAddSequenceElementCommand());
        EXPECT_EQ(object.items.size(), 1u);

        commands.Execute(items->MakeAddSequenceElementCommand());
        EXPECT_EQ(object.items.size(), 2u);
        EXPECT_EQ(items->GetSequenceCount(), 2u);

        EXPECT_TRUE(commands.Undo());
        EXPECT_EQ(object.items.size(), 1u);
        EXPECT_TRUE(commands.Undo());
        EXPECT_EQ(object.items.size(), 0u);

        EXPECT_TRUE(commands.Redo());
        EXPECT_EQ(object.items.size(), 1u);
    }

    TEST(PropertyModelTest, InvalidDescriptorIsSafe)
    {
        PropertyDescriptor descriptor;
        EXPECT_FALSE(descriptor.IsValid());
        EXPECT_FALSE(descriptor.CanEdit());
        EXPECT_FALSE(static_cast<bool>(descriptor.GetValue()));
        EXPECT_EQ(descriptor.MakeEditCommand(Any(1.f)), nullptr);
    }

} // namespace sky::editor
