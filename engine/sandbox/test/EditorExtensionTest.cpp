//
// Created on 2026/09/21.
//

#include <editor/core/actor/EditorActorCreation.h>
#include <editor/core/asset/AssetCreator.h>
#include <editor/core/extension/EditorExtensionHost.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace sky;
using namespace sky::editor;

namespace {

    class DummyAssetCreator : public AssetCreatorBase {
    public:
        std::string GetExtension() const override { return "dummy"; }
        void CreateAsset(const FilePath &) override {}
    };

    class DummyActorCreator : public IActorCreateBase {
    public:
        Name GetGroup() const override { return Name("Test"); }
        Name GetName() const override { return Name("DummyActor"); }
        bool OnCreateActor(Actor *) override { return true; }
    };

    class DummyExtension : public EditorExtension {
    public:
        const char *GetName() const override { return "DummyExtension"; }

        void Register() override
        {
            AssetCreatorManager::Get()->RegisterTool(Name("dummy"), new DummyAssetCreator());
            EditorActorCreation::Get()->RegisterCreation(&actorCreator);
        }

        void Unregister() override
        {
            AssetCreatorManager::Get()->UnRegisterTool(Name("dummy"));
            EditorActorCreation::Get()->UnRegisterCreation(&actorCreator);
        }

    private:
        DummyActorCreator actorCreator;
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
    EXPECT_NE(AssetCreatorManager::Get()->GetTools().find(Name("dummy")),
              AssetCreatorManager::Get()->GetTools().end());
    EXPECT_FALSE(EditorActorCreation::Get()->GetTools().empty());

    host.UnregisterAll();
    EXPECT_FALSE(host.IsRegistered());
    EXPECT_EQ(AssetCreatorManager::Get()->GetTools().count(Name("dummy")), 0u);
    EXPECT_TRUE(EditorActorCreation::Get()->GetTools().empty());
}
