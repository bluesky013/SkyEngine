//
// Created on 2026/10/07.
//

#include <editor/core/document/WorldDocument.h>

#include <framework/serialization/SerializationContext.h>
#include <framework/world/World.h>

#include <filesystem>
#include <gtest/gtest.h>

using namespace sky::editor;

TEST(WorldDocumentTest, EnableDisableAndSaveRoundTrip)
{
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
}

TEST(WorldDocumentTest, CreatePlayWorldDuplicates)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "sky_world_play_test.world";
    std::error_code             ec;
    std::filesystem::remove(path, ec);

    sky::World::Reflect(sky::SerializationContext::Get());

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
