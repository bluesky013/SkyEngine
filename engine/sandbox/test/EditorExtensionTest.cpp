//
// Created on 2026/09/21.
//

#include <editor/core/actor/EditorActorCreation.h>
#include <editor/core/asset/AssetCreator.h>
#include <editor/core/extension/DefaultEditorExtension.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/extension/EditorExtensionHost.h>
#include <editor/core/extension/EditorScopeStack.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

using namespace sky;
using namespace sky::editor;

namespace {

    class DummyAssetCreator : public AssetCreatorBase {
    public:
        std::string GetExtension() const override
        {
            return "dummy";
        }
        void CreateAsset(const FilePath &) override
        {
        }
    };

    class DummyExtension : public EditorExtension {
    public:
        const char *GetName() const override
        {
            return "DummyExtension";
        }

        void Register() override
        {
            AssetCreatorManager::Get()->RegisterTool(Name("dummy"), new DummyAssetCreator());
            EXPECT_TRUE(EditorActorCreation::Get()->RegisterCreation({{}, "dummy", "DummyActor", "Test", 0, [](Actor &) { return true; }}));
        }

        void Unregister() override
        {
            AssetCreatorManager::Get()->UnRegisterTool(Name("dummy"));
            EXPECT_TRUE(EditorActorCreation::Get()->UnRegisterCreation("dummy"));
        }
    };

} // namespace

TEST(EditorExtensionTest, RegisterAndUnregister)
{
    EditorExtensionHost host;
    host.Add(std::make_unique<DummyExtension>());
    EXPECT_EQ(host.GetCount(), 1u);
    EXPECT_NE(host.Find("DummyExtension"), nullptr);
    EXPECT_FALSE(host.IsRegistered());

    host.RegisterAll();
    EXPECT_TRUE(host.IsRegistered());
    EXPECT_NE(AssetCreatorManager::Get()->GetTools().find(Name("dummy")), AssetCreatorManager::Get()->GetTools().end());
    EXPECT_FALSE(EditorActorCreation::Get()->GetTools().empty());

    host.UnregisterAll();
    EXPECT_FALSE(host.IsRegistered());
    EXPECT_EQ(AssetCreatorManager::Get()->GetTools().count(Name("dummy")), 0u);
    EXPECT_TRUE(EditorActorCreation::Get()->GetTools().empty());
}

TEST(EditorExtensionTest, IconDirectoriesAreExposed)
{
    // An extension with no override contributes no icon roots.
    DummyExtension dummy;
    EXPECT_TRUE(dummy.GetIconDirectories().empty());

    class IconExtension : public EditorExtension {
    public:
        const char *GetName() const override
        {
            return "IconExtension";
        }
        void Register() override
        {
        }
        void Unregister() override
        {
        }

        std::vector<std::string> GetIconDirectories() const override
        {
            return {"icons/aurora"};
        }
    };

    EditorExtensionHost host;
    host.Add(std::make_unique<IconExtension>());
    ASSERT_EQ(host.GetExtensions().size(), 1u);
    const std::vector<std::string> dirs = host.GetExtensions().front()->GetIconDirectories();
    ASSERT_EQ(dirs.size(), 1u);
    EXPECT_EQ(dirs.front(), "icons/aurora");
}

TEST(EditorExtensionTest, DefaultExtensionDeclaresBuiltinIconRoot)
{
    PanelRegistry                  registry;
    DefaultEditorExtension         extension(registry);
    const std::vector<std::string> dirs = extension.GetIconDirectories();
    ASSERT_EQ(dirs.size(), 1u);
    EXPECT_EQ(dirs.front(), "icons");
}

TEST(EditorExtensionTest, ExtensionActionsGoThroughRegistry)
{
    // An extension contributes actions from Register() and removes them from Unregister();
    // the host drains extensions before the host's built-in actions are registered, so they
    // survive (see SandboxModule::BuildEditor).
    class ActionExtension : public EditorExtension {
    public:
        const char *GetName() const override
        {
            return "ActionExtension";
        }
        void Register() override
        {
            EditorActionRegistry::Get()->Add({.id = "ext.demo", .label = "Demo", .invoke = []() {}});
        }
        void Unregister() override
        {
            EditorActionRegistry::Get()->Remove(EditorScopeStack::GlobalScope(), "ext.demo");
        }
    };

    EditorActionRegistry::Get()->Clear();
    EditorScopeStack::Get()->Clear();

    EditorExtensionHost host;
    host.Add(std::make_unique<ActionExtension>());
    host.RegisterAll();
    EXPECT_NE(EditorActionRegistry::Get()->Find("ext.demo"), nullptr);

    host.UnregisterAll();
    EXPECT_EQ(EditorActionRegistry::Get()->Find("ext.demo"), nullptr);

    EditorActionRegistry::Get()->Clear();
    EditorScopeStack::Get()->Clear();
}
