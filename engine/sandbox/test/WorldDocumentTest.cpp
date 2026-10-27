//
// Created on 2026/10/07.
//

#include "ReflectionTestEnvironment.h"
#include <editor/core/command/CommandService.h>
#include <editor/core/document/WorldDocument.h>
#include <editor/core/property/PropertyDescriptor.h>
#include <framework/world/WorldSubSystemRegistry.h>

#include <framework/serialization/SerializationContext.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/World.h>

#include <cmath>
#include <filesystem>
#include <gtest/gtest.h>

using namespace sky::editor;

TEST(WorldDocumentTest, EnableDisableAndSaveRoundTrip)
{
    sky::WorldSubSystemRegistration registration;
    registration.factory = [](sky::World &, const sky::Any &) { return std::make_unique<sky::IWorldSubSystem>(); };
    ASSERT_TRUE(sky::WorldSubSystemRegistry::Get().Register(sky::Name("Navigation"), std::move(registration)));
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "sky_world_document_test.world";
    std::error_code             ec;
    std::filesystem::remove(path, ec);

    {
        WorldDocument document(path.string());
        EXPECT_TRUE(document.SetSubSystemEnabled("Navigation", true));
        EXPECT_TRUE(document.SetSubSystemEnabled("Physics", false));
        EXPECT_TRUE(document.IsDirty());
        EXPECT_TRUE(document.Save());
        EXPECT_FALSE(document.IsDirty());
    }

    {
        WorldDocument document(path.string());
        EXPECT_TRUE(document.Load());
        bool enabled = false;
        ASSERT_TRUE(document.IsSubSystemEnabled("Navigation", enabled));
        EXPECT_TRUE(enabled);
        ASSERT_TRUE(document.IsSubSystemEnabled("Physics", enabled));
        EXPECT_FALSE(enabled);
        EXPECT_FALSE(document.IsDirty());
    }

    std::filesystem::remove(path, ec);
    sky::WorldSubSystemRegistry::Get().Unregister(sky::Name("Navigation"));
}

TEST(WorldDocumentTest, CreatePlayWorldDuplicates)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "sky_world_play_test.world";
    std::error_code             ec;
    std::filesystem::remove(path, ec);

    WorldDocument document(path.string());
    sky::World   *edit = document.GetWorld();
    ASSERT_NE(edit, nullptr);
    edit->CreateActor("A");
    edit->CreateActor("B");
    ASSERT_EQ(edit->GetActors().size(), 2u);

    sky::WorldPtr play = document.CreatePlayWorld();
    ASSERT_NE(play, nullptr);
    EXPECT_NE(play.Get(), edit);
    EXPECT_EQ(play->GetActors().size(), 2u);
    EXPECT_EQ(edit->GetActors().size(), 2u); // edit world unchanged

    std::filesystem::remove(path, ec);
}

namespace {
    struct DocumentTestConfig {
        float gravity = -9;
        bool  SetGravity(float value)
        {
            if (!std::isfinite(value) || value > 0)
                return false;
            gravity = value;
            return true;
        }
        float GetGravity() const
        {
            return gravity;
        }
    };
    struct DocumentTestSystem : sky::IWorldSubSystem {
        float gravity = 0;
    };
    const auto *configMetadata = sky::editor::test::AddReflectionEnvironment([] {
        sky::SerializationContext::Get()
            ->Register<DocumentTestConfig>("DocumentTestConfig")
            .Member<&DocumentTestConfig::SetGravity, &DocumentTestConfig::GetGravity>("gravity");
    });
    class WorldConfigCommandTest : public testing::Test {
    protected:
        void SetUp() override
        {
            sky::WorldSubSystemRegistration registration;
            registration.configType        = sky::TypeInfoObj<DocumentTestConfig>::Get()->RtInfo();
            registration.makeDefaultConfig = [] { return sky::Any(std::in_place_type<DocumentTestConfig>); };
            registration.factory           = [](sky::World &, const sky::Any &value) -> std::unique_ptr<sky::IWorldSubSystem> {
                auto system = std::make_unique<DocumentTestSystem>();
                if (const auto *config = value.GetAsConst<DocumentTestConfig>())
                    system->gravity = config->gravity;
                return system;
            };
            ASSERT_TRUE(sky::WorldSubSystemRegistry::Get().Register(sky::Name("DocConfig"), std::move(registration)));
        }
        void TearDown() override
        {
            sky::WorldSubSystemRegistry::Get().Unregister(sky::Name("DocConfig"));
        }
        PropertyDescriptor Property(WorldDocument &document)
        {
            auto       *value = document.EnsureSubSystemConfig("DocConfig");
            const auto *node  = sky::GetTypeNode(*value);
            return {value->Data(), &node->members.at("gravity"), "gravity", ""};
        }
    };
} // namespace

TEST_F(WorldConfigCommandTest, ConfigurationHistoryResolvesAfterVectorGrowthAndRejectsFailedSetter)
{
    WorldDocument  document("");
    CommandService commands;
    auto           property = Property(document);
    ASSERT_TRUE(commands.Execute(document.MakeConfigEditCommand("DocConfig", property, sky::Any(-3.f))));
    for (int i = 0; i < 256; ++i)
        document.SetSubSystemEnabled("Extra" + std::to_string(i), false);
    ASSERT_TRUE(commands.Undo());
    EXPECT_FLOAT_EQ(document.EnsureSubSystemConfig("DocConfig")->GetAs<DocumentTestConfig>()->gravity, -9.f);
    ASSERT_TRUE(commands.Redo());
    EXPECT_FLOAT_EQ(document.EnsureSubSystemConfig("DocConfig")->GetAs<DocumentTestConfig>()->gravity, -3.f);
    EXPECT_FALSE(commands.Execute(document.MakeConfigEditCommand("DocConfig", Property(document), sky::Any(5.f))));
    EXPECT_EQ(commands.GetUndoCount(), 1u);
    EXPECT_FLOAT_EQ(document.EnsureSubSystemConfig("DocConfig")->GetAs<DocumentTestConfig>()->gravity, -3.f);
}

TEST_F(WorldConfigCommandTest, SettingsAndDisableApplyToNextPlayAndEnableIsUndoable)
{
    WorldDocument  document("");
    CommandService commands;
    ASSERT_TRUE(commands.Execute(document.MakeSubSystemEnabledCommand("DocConfig", true)));
    auto property = Property(document);
    ASSERT_TRUE(commands.Execute(document.MakeConfigEditCommand("DocConfig", property, sky::Any(-3.f))));
    auto first = document.CreatePlayWorld();
    ASSERT_TRUE(first);
    const auto *system = static_cast<DocumentTestSystem *>(first->GetSubSystem(sky::Name("DocConfig")));
    ASSERT_NE(system, nullptr);
    EXPECT_FLOAT_EQ(system->gravity, -3.f);
    ASSERT_TRUE(commands.Execute(document.MakeSubSystemEnabledCommand("DocConfig", false)));
    auto second = document.CreatePlayWorld();
    ASSERT_TRUE(second);
    EXPECT_EQ(second->GetSubSystem(sky::Name("DocConfig")), nullptr);
    EXPECT_FLOAT_EQ(system->gravity, -3.f);
    ASSERT_TRUE(commands.Undo());
    ASSERT_TRUE(commands.Undo());
    ASSERT_TRUE(commands.Undo());
    bool enabled = false;
    EXPECT_FALSE(document.IsSubSystemEnabled("DocConfig", enabled));
    ASSERT_TRUE(commands.Redo());
    ASSERT_TRUE(commands.Redo());
    EXPECT_FLOAT_EQ(document.EnsureSubSystemConfig("DocConfig")->GetAs<DocumentTestConfig>()->gravity, -3.f);
}

TEST_F(WorldConfigCommandTest, ReloadInvalidatesOldConfigCommands)
{
    const auto     path = std::filesystem::temp_directory_path() / ("world-config-" + sky::Uuid::Create().ToString() + ".world");
    WorldDocument  document(path.string());
    CommandService commands;
    auto           property = Property(document);
    ASSERT_TRUE(commands.Execute(document.MakeConfigEditCommand("DocConfig", property, sky::Any(-3.f))));
    ASSERT_TRUE(document.Save());
    ASSERT_TRUE(document.Load());
    EXPECT_FALSE(commands.Undo());
    EXPECT_FLOAT_EQ(document.EnsureSubSystemConfig("DocConfig")->GetAs<DocumentTestConfig>()->gravity, -3.f);
    std::error_code error;
    std::filesystem::remove(path, error);
}

TEST(WorldDocumentTest, MissingSubsystemBlocksLoadAndPlayWithoutReplacingDocument)
{
    const auto    path = std::filesystem::temp_directory_path() / ("world-missing-" + sky::Uuid::Create().ToString() + ".world");
    WorldDocument document(path.string());
    document.SetSubSystemEnabled("UnregisteredPlugin", true);
    auto *before = document.GetWorld();
    ASSERT_TRUE(document.Save());
    EXPECT_FALSE(document.Load());
    EXPECT_EQ(document.GetWorld(), before);
    EXPECT_FALSE(document.CreatePlayWorld());
    EXPECT_NE(document.GetLastError().find("UnregisteredPlugin"), std::string::npos);
    std::error_code error;
    std::filesystem::remove(path, error);
}
