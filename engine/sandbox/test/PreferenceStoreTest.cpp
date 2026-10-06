//
// Created on 2026/10/06.
//

#include <editor/core/preferences/PreferenceStore.h>

#include <gtest/gtest.h>
#include <string>

using namespace sky::editor;

namespace {

    PreferenceRegistry MakeRegistry()
    {
        PreferenceRegistry registry;

        PreferenceSection generalSection;
        generalSection.id    = "general.main";
        generalSection.title = "General";
        generalSection.entries.push_back({"general.autosave", "Autosave", PreferenceValue::Bool(true)});
        generalSection.entries.push_back({"general.theme", "Theme", PreferenceValue::Str("dark"), 0.0, 0.0, {"dark", "light"}});

        PreferencePage general;
        general.id       = "general";
        general.title    = "General";
        general.sections = {generalSection};
        registry.RegisterPage(general);

        PreferenceSection editorSection;
        editorSection.id    = "editor.main";
        editorSection.title = "Editor";
        editorSection.entries.push_back({"editor.gridSize", "Grid Size", PreferenceValue::Float(10.0), 1.0, 100.0});
        editorSection.entries.push_back({"editor.undoDepth", "Undo Depth", PreferenceValue::Int(32)});

        PreferencePage editor;
        editor.id       = "editor";
        editor.title    = "Editor";
        editor.sections = {editorSection};
        registry.RegisterPage(editor);

        return registry;
    }

} // namespace

TEST(PreferenceStoreTest, RegistryOrderAndDefaults)
{
    PreferenceRegistry registry = MakeRegistry();
    ASSERT_EQ(registry.GetPages().size(), 2u);
    EXPECT_EQ(registry.GetPages()[0].id, "general");
    EXPECT_EQ(registry.GetPages()[1].id, "editor");

    PreferenceStore store(&registry);
    bool            autosave = false;
    std::string     theme;
    int64_t         undo = 0;
    EXPECT_TRUE(store.GetBool("general.autosave", autosave));
    EXPECT_TRUE(autosave);
    EXPECT_TRUE(store.GetString("general.theme", theme));
    EXPECT_EQ(theme, "dark");
    EXPECT_TRUE(store.GetInt("editor.undoDepth", undo));
    EXPECT_EQ(undo, 32);
    EXPECT_FALSE(store.IsDirty());
}

TEST(PreferenceStoreTest, TypeSafetyAndUnknownKeys)
{
    PreferenceRegistry registry = MakeRegistry();
    PreferenceStore    store(&registry);

    EXPECT_TRUE(store.SetBool("general.autosave", false));
    EXPECT_TRUE(store.IsDirty());

    // Wrong type and unknown key are rejected.
    EXPECT_FALSE(store.SetFloat("general.autosave", 1.0));
    EXPECT_FALSE(store.SetString("does.not.exist", "x"));
}

TEST(PreferenceStoreTest, CommitRevertAndReset)
{
    PreferenceRegistry registry = MakeRegistry();
    PreferenceStore    store(&registry);

    EXPECT_TRUE(store.SetInt("editor.undoDepth", 64));
    EXPECT_TRUE(store.IsDirty());
    store.Commit();
    EXPECT_FALSE(store.IsDirty());

    EXPECT_TRUE(store.SetInt("editor.undoDepth", 8));
    store.Revert();
    int64_t undo = 0;
    EXPECT_TRUE(store.GetInt("editor.undoDepth", undo));
    EXPECT_EQ(undo, 64);

    EXPECT_TRUE(store.SetInt("editor.undoDepth", 8));
    EXPECT_TRUE(store.SetBool("general.autosave", false));
    EXPECT_TRUE(store.ResetPageToDefaults("editor"));
    EXPECT_TRUE(store.GetInt("editor.undoDepth", undo));
    EXPECT_EQ(undo, 32); // editor reset
    bool autosave = true;
    EXPECT_TRUE(store.GetBool("general.autosave", autosave));
    EXPECT_FALSE(autosave); // general untouched
}

TEST(PreferenceStoreTest, JsonRoundTrip)
{
    PreferenceRegistry registry = MakeRegistry();
    PreferenceStore    store(&registry);
    store.SetBool("general.autosave", false);
    store.SetString("general.theme", "light");
    store.SetInt("editor.undoDepth", 100);
    store.Commit();

    const std::string json = store.ToJson();

    PreferenceStore loaded(&registry);
    ASSERT_TRUE(loaded.FromJson(json));
    bool        autosave = true;
    std::string theme;
    int64_t     undo = 0;
    EXPECT_TRUE(loaded.GetBool("general.autosave", autosave));
    EXPECT_FALSE(autosave);
    EXPECT_TRUE(loaded.GetString("general.theme", theme));
    EXPECT_EQ(theme, "light");
    EXPECT_TRUE(loaded.GetInt("editor.undoDepth", undo));
    EXPECT_EQ(undo, 100);
    EXPECT_FALSE(loaded.IsDirty());
}

TEST(PreferenceStoreTest, ForwardCompatibleLoad)
{
    PreferenceRegistry registry = MakeRegistry();
    PreferenceStore    store(&registry);

    const std::string json = R"({
        "version": 1,
        "values": {
            "general.autosave": { "type": "bool", "value": false },
            "future.unknown":   { "type": "bool", "value": true }
        }
    })";

    ASSERT_TRUE(store.FromJson(json));
    bool autosave = true;
    EXPECT_TRUE(store.GetBool("general.autosave", autosave));
    EXPECT_FALSE(autosave);
    EXPECT_EQ(store.FindWorking("future.unknown"), nullptr);
}
