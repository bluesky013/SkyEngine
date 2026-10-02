//
// Created by blues on 2026/10/2.
//

#include <gtest/gtest.h>

#include <framework/asset/AssetIndexFile.h>
#include <core/file/FileSystem.h>

#include <filesystem>

using namespace sky;

TEST(AssetIndexFileTest, RoundTripWithExtra)
{
    AssetIndexFile file("file", "cook");

    IndexFileEntry a;
    a.key = "rock.png";
    a.id = Uuid::Create();
    a.extra = R"({"targets":["pc_bc"]})";
    file.Set(a);

    IndexFileEntry b;
    b.key = "wood.png";
    b.id = Uuid::Create();
    file.Set(b);

    AssetIndexFile parsed("file", "cook");
    parsed.Parse(file.Serialize());

    ASSERT_EQ(parsed.Entries().size(), 2U);
    const auto *pa = parsed.Find("rock.png");
    ASSERT_NE(pa, nullptr);
    EXPECT_TRUE(pa->id == a.id);
    EXPECT_EQ(pa->extra, a.extra);
    const auto *pb = parsed.Find("wood.png");
    ASSERT_NE(pb, nullptr);
    EXPECT_TRUE(pb->id == b.id);
    EXPECT_TRUE(pb->extra.empty());
}

TEST(AssetIndexFileTest, SortedByKey)
{
    AssetIndexFile file("file");
    for (const char *name : { "c.mat", "a.mat", "b.mat" }) {
        IndexFileEntry entry;
        entry.key = name;
        entry.id = Uuid::Create();
        file.Set(entry);
    }

    const auto &entries = file.Entries();
    ASSERT_EQ(entries.size(), 3U);
    EXPECT_EQ(entries[0].key, "a.mat");
    EXPECT_EQ(entries[1].key, "b.mat");
    EXPECT_EQ(entries[2].key, "c.mat");
}

TEST(AssetIndexFileTest, DuplicateReplaces)
{
    AssetIndexFile file("file");

    IndexFileEntry entry;
    entry.key = "x.mat";
    entry.id = Uuid::Create();
    file.Set(entry);
    const Uuid first = entry.id;

    entry.id = Uuid::Create();
    file.Set(entry);

    ASSERT_EQ(file.Entries().size(), 1U);
    const auto *found = file.Find("x.mat");
    ASSERT_NE(found, nullptr);
    EXPECT_FALSE(found->id == first);
}

TEST(AssetIndexFileTest, MalformedLinesSkipped)
{
    AssetIndexFile file("file");
    const std::string text =
        "not json\n"
        "{\"file\":\"ok.mat\",\"id\":\"3f9a1c2e-4b8d-4a11-a1b2-2f6e9c0d8e77\"}\n"
        "{\"file\":\"bad.mat\"}\n";

    file.Parse(text);

    ASSERT_EQ(file.Entries().size(), 1U);
    EXPECT_NE(file.Find("ok.mat"), nullptr);
}

TEST(AssetIndexFileTest, ProductPathKey)
{
    AssetIndexFile file("path");

    IndexFileEntry entry;
    entry.key = "textures/rock.png";
    entry.id = Uuid::Create();
    file.Set(entry);

    AssetIndexFile parsed("path");
    parsed.Parse(file.Serialize());
    ASSERT_EQ(parsed.Entries().size(), 1U);
    EXPECT_NE(parsed.Find("textures/rock.png"), nullptr);
}

TEST(AssetIndexFileTest, FileRoundTrip)
{
    const auto root = std::filesystem::temp_directory_path() / "sky_asset_index_file_test";
    std::filesystem::remove_all(root);

    FileSystemPtr fs = new NativeFileSystem(FilePath(root.string()));

    AssetIndexFile file("path");
    IndexFileEntry entry;
    entry.key = "materials/wood.mat";
    entry.id = Uuid::Create();
    file.Set(entry);

    ASSERT_TRUE(AssetIndexFile::Save(fs, FilePath("product.index"), file));

    const auto loaded = AssetIndexFile::Load(fs, FilePath("product.index"), "path");
    ASSERT_EQ(loaded.Entries().size(), 1U);
    const auto *found = loaded.Find("materials/wood.mat");
    ASSERT_NE(found, nullptr);
    EXPECT_TRUE(found->id == entry.id);

    std::filesystem::remove_all(root);
}

TEST(AssetIndexFileTest, CacheInvalidate)
{
    const auto root = std::filesystem::temp_directory_path() / "sky_asset_index_cache_test";
    std::filesystem::remove_all(root);

    FileSystemPtr fs = new NativeFileSystem(FilePath(root.string()));
    fs->MakeDir(FilePath("d"));

    AssetIndexFile file("file", "cook");
    IndexFileEntry entry;
    entry.key = "a.mat";
    entry.id = Uuid::Create();
    file.Set(entry);

    AssetIndexFileCache cache{"assets.jsonl", "file", "cook"};
    ASSERT_TRUE(cache.Save(fs, FilePath("d"), file));
    ASSERT_EQ(cache.Get(fs, FilePath("d")).Entries().size(), 1U);

    cache.Invalidate(fs, FilePath("d"));
    ASSERT_EQ(cache.Get(fs, FilePath("d")).Entries().size(), 1U);

    std::filesystem::remove_all(root);
}

TEST(AssetIndexFileTest, CanonicalPath)
{
    EXPECT_EQ(MakeCanonicalPath("textures\\rock.png"), "textures/rock.png");
    EXPECT_EQ(MakeCanonicalPath("textures/rock.png"), "textures/rock.png");
}
