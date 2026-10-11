//
// Created on 2026/10/07.
//

#include <editor/core/preferences/EditorSettings.h>
#include <editor/core/preferences/PreferencePageRegistry.h>
#include <editor/core/property/PropertyEditor.h>
#include <editor/core/property/ReflectedForm.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

using namespace sky::editor;

// The reflected settings types register cleanly and build a form whose fields are the struct's
// members (so the Preferences dialog can render them generically, without per-type controls).
TEST(EditorSettingsTest, RegistersReflectedPages)
{
    RegisterEditorSettings(sky::SerializationContext::Get());

    GeneralSettings      general;
    const sky::TypeNode *type = sky::GetTypeNode(sky::TypeInfo<GeneralSettings>::RegisteredId());
    ASSERT_NE(type, nullptr);

    ReflectedForm          form;
    PropertyEditorRegistry editorRegistry;
    form.Build(PropertyObject{&general, type}, editorRegistry);

    ASSERT_FALSE(form.GetSections().empty());
    const std::vector<PropertyField> &fields = form.GetSections().front().fields;
    ASSERT_EQ(fields.size(), 3u); // language, theme, autosave

    const auto findField = [&fields](const std::string &label) -> const PropertyField * {
        for (const PropertyField &field : fields) {
            if (field.control.label == label) {
                return &field;
            }
        }
        return nullptr;
    };
    const PropertyField *language = findField("Language");
    const PropertyField *autosave = findField("Autosave");
    ASSERT_NE(language, nullptr);
    ASSERT_NE(autosave, nullptr);
    EXPECT_EQ(language->kind, PropertyEditorKind::Enum);
    EXPECT_EQ(autosave->kind, PropertyEditorKind::Bool);
}

TEST(EditorSettingsTest, PageRegistryRoundTrip)
{
    PreferencePageRegistry *pages = PreferencePageRegistry::Get();
    pages->Clear();

    EditorViewSettings settings;
    PreferencePage     page;
    page.id     = "editor";
    page.title  = "Editor";
    page.object = &settings;
    page.type   = sky::GetTypeNode(sky::TypeInfo<EditorViewSettings>::RegisteredId());
    pages->Add(page);

    ASSERT_EQ(pages->GetPages().size(), 1u);
    EXPECT_EQ(pages->GetPages().front().title, "Editor");
    EXPECT_NE(pages->GetPages().front().type, nullptr);

    pages->Clear();
    EXPECT_TRUE(pages->IsEmpty());
}
