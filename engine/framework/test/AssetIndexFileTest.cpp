//
// Created by blues on 2026/10/2.
//

#include <gtest/gtest.h>

#include <framework/asset/AssetIndexFile.h>
#include <core/file/FileSystem.h>

#include <atomic>
#include <filesystem>
#include <thread>

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
    ASSERT_EQ(cache.Get(fs, FilePath("d"))->Entries().size(), 1U);

    cache.Invalidate(fs, FilePath("d"));
    ASSERT_EQ(cache.Get(fs, FilePath("d"))->Entries().size(), 1U);

    std::filesystem::remove_all(root);
}

TEST(AssetIndexFileTest, CanonicalPath)
{
    EXPECT_EQ(MakeCanonicalPath("textures\\rock.png"), "textures/rock.png");
    EXPECT_EQ(MakeCanonicalPath("textures/rock.png"), "textures/rock.png");
}

TEST(AssetIndexFileTest, PartialParsingQuarantinesEveryConflictAndPreservesLineNumbers)
{
    const auto     id    = Uuid::Create().ToString();
    const auto     other = Uuid::Create().ToString();
    AssetIndexFile file("file");
    file.ParsePartial("\n{\"file\":\"a.mat\",\"id\":\"" + id + "\"}\n\n" + "{\"file\":\"b.mat\",\"id\":\"" + id + "\"}\ninvalid\n" +
                      "{\"file\":\"c.mat\",\"id\":\"" + other + "\"}\n");
    ASSERT_FALSE(file.IsComplete());
    ASSERT_EQ(file.Errors().size(), 3U);
    EXPECT_EQ(file.Errors()[0].line, 2U);
    EXPECT_EQ(file.Errors()[1].line, 4U);
    EXPECT_EQ(file.Errors()[2].line, 5U);
    ASSERT_EQ(file.Entries().size(), 1U);
    EXPECT_EQ(file.Entries()[0].key, "c.mat");
    EXPECT_FALSE(AssetIndexFile::Save({}, "assets.jsonl", file));
}

TEST(AssetIndexFileTest, StrictParseFailurePreservesExistingOutput)
{
    AssetIndexFile file("file");
    const auto     id = Uuid::Create();
    file.Set({"old.mat", id, {}});
    EXPECT_FALSE(file.ParseChecked("bad json\n"));
    ASSERT_NE(file.Find("old.mat"), nullptr);
    EXPECT_EQ(file.Find("old.mat")->id, id);
    EXPECT_TRUE(file.IsComplete());
}

TEST(AssetIndexFileTest, SnapshotsSurviveReplacementAndReloadDoesNotPublish)
{
    const auto          root = std::filesystem::temp_directory_path() / ("sky_index_snapshot_" + Uuid::Create().ToString());
    FileSystemPtr       fs   = new NativeFileSystem(FilePath(root));
    AssetIndexFileCache cache("assets.jsonl", "file");
    AssetIndexFile      original("file");
    const auto          id = Uuid::Create();
    original.Set({"first.mat", id, {}});
    ASSERT_TRUE(cache.Save(fs, {}, original));
    const auto     retained = cache.Get(fs, {});
    AssetIndexFile next("file");
    next.Set({"second.mat", Uuid::Create(), {}});
    ASSERT_TRUE(AssetIndexFile::Save(fs, "assets.jsonl", next));
    AssetIndexFile reloaded("file");
    ASSERT_TRUE(cache.Reload(fs, {}, reloaded));
    EXPECT_NE(reloaded.Find("second.mat"), nullptr);
    EXPECT_EQ(cache.Get(fs, {}), retained);
    ASSERT_TRUE(cache.Save(fs, {}, next));
    EXPECT_NE(cache.Get(fs, {})->Find("second.mat"), nullptr);
    cache.Invalidate(fs, {});
    cache.Clear();
    EXPECT_EQ(retained->Find("first.mat")->id, id);
    EXPECT_NE(cache.Get(fs, {})->Find("second.mat"), nullptr);
    next.ParsePartial("broken");
    EXPECT_FALSE(cache.Save(fs, {}, next));
    EXPECT_NE(cache.Get(fs, {})->Find("second.mat"), nullptr);
    std::filesystem::remove_all(root);
}

TEST(AssetIndexFileTest, ConcurrentReadersRetainSnapshotsDuringSaveAndInvalidation)
{
    const auto          root = std::filesystem::temp_directory_path() / ("sky_index_concurrent_" + Uuid::Create().ToString());
    FileSystemPtr       fs   = new NativeFileSystem(FilePath(root));
    AssetIndexFileCache cache("product.index", "path");
    AssetIndexFile      file("path");
    file.Set({"asset.world", Uuid::Create(), {}});
    ASSERT_TRUE(cache.Save(fs, {}, file));
    std::atomic<bool>   done  = false;
    std::atomic<size_t> reads = 0;
    std::thread         reader([&] {
        do {
            const auto  snapshot = cache.Get(fs, {});
            const auto *entry    = snapshot->Find("asset.world");
            ASSERT_NE(entry, nullptr);
            const auto id = entry->id;
            std::this_thread::yield();
            EXPECT_EQ(entry->id, id);
            ++reads;
        } while (!done);
    });
    for (size_t i = 0; i < 20; ++i) {
        file.Set({"asset.world", Uuid::Create(), {}});
        EXPECT_TRUE(cache.Save(fs, {}, file));
        cache.Invalidate(fs, {});
        cache.Clear();
    }
    done = true;
    reader.join();
    EXPECT_GT(reads.load(), 0U);
    std::filesystem::remove_all(root);
}

TEST(AssetIndexFileTest, FailedAtomicReplacementDoesNotPublishCandidate)
{
    class FailingFileSystem final : public NativeFileSystem {
    public:
        explicit FailingFileSystem(const FilePath &path) : NativeFileSystem(path)
        {
        }
        FileOperationStatus ReplaceFileAtomic(const FilePath &from, const FilePath &to) const override
        {
            return fail ? FileOperationStatus{FileOperationError::IOError, "injected replacement failure"}
                        : NativeFileSystem::ReplaceFileAtomic(from, to);
        }
        FileOperationStatus RemoveFile(const FilePath &path) const override
        {
            return failCleanup ? FileOperationStatus{FileOperationError::IOError, "injected temporary cleanup failure"}
                               : NativeFileSystem::RemoveFile(path);
        }
        bool fail        = false;
        bool failCleanup = false;
    };
    const auto                    root = std::filesystem::temp_directory_path() / ("sky_index_failure_" + Uuid::Create().ToString());
    CounterPtr<FailingFileSystem> fs   = new FailingFileSystem(FilePath(root));
    AssetIndexFileCache           cache("assets.jsonl", "file");
    AssetIndexFile                candidate("file");
    const auto                    id = Uuid::Create();
    candidate.Set({"asset.world", id, {}});
    ASSERT_TRUE(cache.Save(fs, {}, candidate));
    const auto original = cache.Get(fs, {});
    candidate.Set({"asset.world", Uuid::Create(), {}});
    fs->fail            = true;
    const auto rejected = cache.Save(fs, {}, candidate);
    EXPECT_FALSE(rejected);
    EXPECT_TRUE(rejected.recovery.empty());
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator(root), std::filesystem::directory_iterator{}), 1);
    fs->failCleanup     = true;
    const auto retained = cache.Save(fs, {}, candidate);
    EXPECT_FALSE(retained);
    ASSERT_EQ(retained.recovery.size(), 1U);
    EXPECT_EQ(retained.recovery.front().stage, "remove-temporary");
    EXPECT_TRUE(retained.recovery.front().path.Exist());
    EXPECT_EQ(cache.Get(fs, {}), original);
    cache.Clear();
    EXPECT_EQ(cache.Get(fs, {})->Find("asset.world")->id, id);
    std::filesystem::remove_all(root);
}

TEST(AssetIndexFileTest, FilesystemInstancesAtSamePathHaveSeparateCacheSlots)
{
    const auto          root   = std::filesystem::temp_directory_path() / ("sky_index_mount_" + Uuid::Create().ToString());
    FileSystemPtr       first  = new NativeFileSystem(FilePath(root));
    FileSystemPtr       second = new NativeFileSystem(FilePath(root));
    AssetIndexFileCache cache("assets.jsonl", "file");
    AssetIndexFile      file("file");
    const auto          original = Uuid::Create();
    file.Set({"asset.world", original, {}});
    ASSERT_TRUE(cache.Save(first, {}, file));
    const auto replacement = Uuid::Create();
    file.Set({"asset.world", replacement, {}});
    ASSERT_TRUE(AssetIndexFile::Save(second, "assets.jsonl", file));
    EXPECT_EQ(cache.Get(first, {})->Find("asset.world")->id, original);
    EXPECT_EQ(cache.Get(second, {})->Find("asset.world")->id, replacement);
    std::filesystem::remove_all(root);
}
