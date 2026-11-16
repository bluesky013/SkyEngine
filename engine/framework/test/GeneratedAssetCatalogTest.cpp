#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetDependencyProvider.h>
#include <framework/asset/AssetExecutor.h>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <thread>

using namespace sky;

namespace {
    class ManifestFileSystem : public NativeFileSystem {
    public:
        using NativeFileSystem::NativeFileSystem;
        bool                             fail        = false;
        bool                             failRemoval = false;
        mutable std::string              failManifestOnce;
        mutable std::vector<std::string> lockOrder;
        bool                             failRead = false;
        mutable std::function<void()>    afterCommit;
        std::function<void()>            beforeSourceOpen;
        bool                             failDestinationRestore = false;
        bool                             failRenameRestore      = false;
        bool                             Rename(const FilePath &from, const FilePath &to) const override
        {
            return failRenameRestore && to.GetStr() == "character.model" ? false : NativeFileSystem::Rename(from, to);
        }
        FilePtr OpenFile(const FilePath &path) override
        {
            if (path.GetStr() == "character.model" && beforeSourceOpen) {
                const auto callback = std::move(beforeSourceOpen);
                beforeSourceOpen    = {};
                callback();
            }
            return failRead && path.FileName() == "assets.jsonl" ? FilePtr{} : NativeFileSystem::OpenFile(path);
        }
        FileOperationStatus ReplaceFileAtomic(const FilePath &from, const FilePath &to) const override
        {
            if (failDestinationRestore && to.GetStr() == "moved/assets.jsonl") {
                std::string contents;
                const auto  file = const_cast<ManifestFileSystem *>(this)->NativeFileSystem::OpenFile(from);
                if (file && file->ReadString(contents) && contents.find("character.model") == std::string::npos)
                    return {FileOperationError::IOError, "injected manifest restoration failure"};
            }
            if (!failManifestOnce.empty() && to.GetStr() == failManifestOnce) {
                failManifestOnce.clear();
                return {FileOperationError::IOError, "injected manifest failure"};
            }
            if (fail)
                return {FileOperationError::IOError, "injected manifest failure"};
            const auto status = NativeFileSystem::ReplaceFileAtomic(from, to);
            if (status && afterCommit) {
                const auto callback = std::move(afterCommit);
                afterCommit         = {};
                callback();
            }
            return status;
        }
        FileOperationStatus RemoveFile(const FilePath &path) const override
        {
            if (failRemoval && path.GetStr().find(".deleted-") != std::string::npos) {
                return {FileOperationError::IOError, "injected source removal failure"};
            }
            return NativeFileSystem::RemoveFile(path);
        }
        FileOperationStatus AcquireFileLock(const FilePath &path, FileLockPtr &out) const override
        {
            lockOrder.push_back(path.GetStr());
            return NativeFileSystem::AcquireFileLock(path, out);
        }
    };

    class GeneratedAssetCatalogTest : public testing::Test {
    protected:
        void SetUp() override
        {
            std::error_code error;
            root = std::filesystem::temp_directory_path(error) / ("sky-generated-assets-" + Uuid::Create().ToString());
            ASSERT_FALSE(error);
            filesystem = new ManifestFileSystem(FilePath(root));
            database.SetWorkSpaceFs(filesystem);
            ASSERT_TRUE(filesystem->WriteFileExclusive("character.model", std::array<uint8_t, 1>{1}));
            source = database.RegisterAsset("character.model", false);
            ASSERT_NE(source, nullptr);
        }
        void TearDown() override
        {
            database.Reset();
            std::error_code error;
            std::filesystem::remove_all(root, error);
            EXPECT_FALSE(error) << error.message();
        }
        GeneratedAssetManifest Generate()
        {
            GeneratedAssetManifest result;
            EXPECT_TRUE(database.UpdateGeneratedProducts(source->uuid, descriptors, defaults, result));
            return result;
        }
        std::filesystem::path                   root;
        CounterPtr<ManifestFileSystem>          filesystem;
        AssetDataBase                           database;
        AssetSourcePtr                          source;
        std::array<GeneratedAssetDescriptor, 2> descriptors{{{"binding/body", "AuroraSkin"}, {"rig/main", "Skeleton"}}};
        std::map<std::string, std::string>      defaults{{"mesh", "$source"}, {"skin", "binding/body"}, {"skeleton", "rig/main"}};
    };
} // namespace

TEST_F(GeneratedAssetCatalogTest, RecookMoveAndReloadKeepChildrenAndDefaultCombination)
{
    const auto first = Generate();
    ASSERT_EQ(first.records.size(), 2U);
    const auto second = Generate();
    EXPECT_EQ(first.records[0].uuid, second.records[0].uuid);
    EXPECT_EQ(first.records[1].uuid, second.records[1].uuid);
    Uuid        owner;
    std::string type, path;
    ASSERT_TRUE(database.GetOwner(first.records[0].uuid, owner));
    EXPECT_EQ(owner, source->uuid);
    ASSERT_TRUE(database.GetType(first.records[0].uuid, type));
    EXPECT_EQ(type, "AuroraSkin");
    std::map<std::string, Uuid> selected;
    ASSERT_TRUE(database.GetGeneratedDefaults(owner, selected));
    EXPECT_EQ(selected["mesh"], owner);
    EXPECT_EQ(selected["skin"], first.records[0].uuid);
    EXPECT_EQ(selected["skeleton"], first.records[1].uuid);
    ASSERT_TRUE(database.SetCookJson(owner, R"({"targets":["test"]})"));
    ASSERT_TRUE(database.MoveAsset("character.model", "moved/character.model"));
    ASSERT_TRUE(database.GetSourcePath(first.records[0].uuid, path));
    EXPECT_EQ(FilePath(path), FilePath("moved/character.model"));
    const auto id = owner;
    database.Reset();
    source = database.RegisterAsset("moved/character.model", false);
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(source->uuid, id);
    EXPECT_TRUE(database.Exists(first.records[0].uuid));
    EXPECT_TRUE(database.Exists(first.records[1].uuid));
    EXPECT_FALSE(database.GetCookJson(id).empty());
    EXPECT_EQ(Generate().records[0].uuid, first.records[0].uuid);
}

TEST_F(GeneratedAssetCatalogTest, DuplicateGetsNewChildIdsAndRemovalKeepsTombstone)
{
    const auto original = Generate();
    ASSERT_EQ(original.records.size(), 2U);
    const auto copy = database.DuplicateAsset(source->path, "copy.model");
    ASSERT_EQ(copy.state, AssetCommitState::Committed);
    ASSERT_NE(copy.source, nullptr);
    EXPECT_NE(copy.source->uuid, source->uuid);
    std::map<std::string, Uuid> copied;
    ASSERT_TRUE(database.GetGeneratedDefaults(copy.source->uuid, copied));
    EXPECT_NE(copied["skin"], original.records[0].uuid);
    EXPECT_NE(copied["skeleton"], original.records[1].uuid);
    EXPECT_EQ(copied["mesh"], copy.source->uuid);
    const std::array<GeneratedAssetDescriptor, 2> renamed{{{"binding/renamed", "AuroraSkin"}, descriptors[1]}};
    auto                                          changedDefaults = defaults;
    changedDefaults["skin"]                                       = "binding/renamed";
    GeneratedAssetManifest changed;
    ASSERT_TRUE(database.UpdateGeneratedProducts(source->uuid, renamed, changedDefaults, changed));
    EXPECT_FALSE(database.Exists(original.records[0].uuid));
    GeneratedAssetRecord old;
    ASSERT_TRUE(database.GetGenerated(original.records[0].uuid, old));
    EXPECT_FALSE(old.active);
    EXPECT_TRUE(database.Exists(original.records[1].uuid));
    Uuid owner;
    EXPECT_FALSE(database.GetOwner(old.uuid, owner));
    ASSERT_TRUE(database.RemoveAsset(copy.source->uuid));
    EXPECT_FALSE(database.Exists(copied["skin"]));
}

TEST_F(GeneratedAssetCatalogTest, FailedSaveAndInvalidSchemaDoNotReplaceLiveCatalog)
{
    const auto original = Generate();
    ASSERT_EQ(original.records.size(), 2U);
    filesystem->fail = true;
    GeneratedAssetManifest                        rejected;
    const std::array<GeneratedAssetDescriptor, 1> replacement{{{"binding/new", "AuroraSkin"}}};
    EXPECT_EQ(database.UpdateGeneratedProducts(source->uuid, replacement, {}, rejected).error, ProductError::IOError);
    EXPECT_TRUE(database.Exists(original.records[0].uuid));
    EXPECT_TRUE(rejected.records.empty());
    filesystem->fail = false;
    auto invalid     = descriptors;
    invalid[0].type  = "WrongType";
    EXPECT_EQ(database.UpdateGeneratedProducts(source->uuid, invalid, defaults, rejected).error, ProductError::InvalidData);
    EXPECT_FALSE(ParseGeneratedAssets(source->uuid, R"({"version":9,"children":[],"defaults":{}})", rejected));
    EXPECT_FALSE(ParseGeneratedAssets(source->uuid, R"({"version":1,"children":[],"defaults":{"skin":"missing"}})", rejected));
    EXPECT_TRUE(database.RefreshOwner(source->uuid));
    EXPECT_TRUE(database.Exists(original.records[0].uuid));
    AssetDependencyGraph graph;
    database.BuildDependencyGraph(graph);
    EXPECT_EQ(graph.Dependencies(original.records[0].uuid), std::vector<Uuid>{source->uuid});
}

TEST_F(GeneratedAssetCatalogTest, ReadOnlySourceUsesCommittedChildIdentitiesWithoutWriting)
{
    ASSERT_TRUE(filesystem->WriteFileExclusive("assets/engine.model", std::array<uint8_t, 1>{1}));
    database.Reset();
    database.SetWorkSpaceFs({});
    database.SetEngineFs(filesystem);
    const auto engine = database.RegisterAsset("engine.model", false);
    ASSERT_NE(engine, nullptr);
    GeneratedAssetManifest empty;
    EXPECT_TRUE(database.UpdateGeneratedProducts(engine->uuid, {}, {{"mesh", "$source"}}, empty));
    EXPECT_EQ(database.UpdateGeneratedProducts(engine->uuid, descriptors, defaults, empty).error, ProductError::Unsupported);
    GeneratedAssetManifest committed;
    ASSERT_TRUE(ReconcileGeneratedAssets(engine->uuid, {}, descriptors, defaults, committed));
    const auto json =
        "{\"file\":\"engine.model\",\"id\":\"" + engine->uuid.ToString() + "\",\"generated\":" + SerializeGeneratedAssets(committed) + "}\n";
    ASSERT_TRUE(
        filesystem->WriteFileExclusive("assets/assets.jsonl", std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(json.data()), json.size())));
    ASSERT_TRUE(database.RefreshOwner(engine->uuid));
    GeneratedAssetManifest recooked;
    ASSERT_TRUE(database.UpdateGeneratedProducts(engine->uuid, descriptors, defaults, recooked));
    ASSERT_EQ(recooked.records.size(), committed.records.size());
    EXPECT_EQ(recooked.records[0].uuid, committed.records[0].uuid);
    EXPECT_FALSE(filesystem->FileExist("assets/assets.jsonl.lock"));
}

TEST(GeneratedAssetCatalog, ConcurrentOwnerCookRetainsEveryCompletionSubscriber)
{
    class HeldBuilder : public AssetBuilder {
    public:
        std::promise<void> started;
        std::promise<void> release;
        uint32_t           requests = 0;
        void               Request(const AssetBuildRequest &, AssetBuildResult &result) override
        {
            ++requests;
            started.set_value();
            release.get_future().wait();
            result.retCode = AssetBuildRetCode::SUCCESS;
        }
        const std::vector<std::string> &GetExtensions() const override
        {
            static const std::vector<std::string> extensions{".generated-test"};
            return extensions;
        }
    };
    AssetBuilderManager manager;
    auto               *builder = new HeldBuilder;
    auto                started = builder->started.get_future();
    manager.RegisterBuilder(builder);
    AssetBuildRequest request;
    request.assetInfo       = new AssetSourceInfo;
    request.assetInfo->uuid = Uuid::Create();
    request.assetInfo->ext  = ".generated-test";
    std::promise<AssetBuildRetCode> first, second;
    auto                            firstResult  = first.get_future();
    auto                            secondResult = second.get_future();
    manager.BuildRequest(request, [&first](const AssetBuildResult &result) { first.set_value(result.retCode); });
    const bool running = started.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    EXPECT_TRUE(running);
    manager.BuildRequest(request, [&second](const AssetBuildResult &result) { second.set_value(result.retCode); });
    builder->release.set_value();
    ASSERT_EQ(firstResult.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    ASSERT_EQ(secondResult.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_EQ(firstResult.get(), AssetBuildRetCode::SUCCESS);
    EXPECT_EQ(secondResult.get(), AssetBuildRetCode::SUCCESS);
    AssetExecutor::Get()->WaitForAll();
    EXPECT_EQ(builder->requests, 1U);
}

TEST_F(GeneratedAssetCatalogTest, FailedRemovalRetainsIdentityAndGeneratedProducts)
{
    const auto children = Generate();
    filesystem->fail    = true;
    const auto status   = database.RemoveAsset(source->uuid);
    EXPECT_EQ(status.status.error, FileOperationError::IOError);
    EXPECT_EQ(database.FindAsset(source->uuid).Get(), source.Get());
    EXPECT_EQ(database.FindAsset(source->path).Get(), source.Get());
    GeneratedAssetRecord child;
    ASSERT_TRUE(database.GetGenerated(children.records.front().uuid, child));
    EXPECT_TRUE(child.active);
    filesystem->fail = false;
    EXPECT_TRUE(database.RemoveAsset(source->uuid));
    EXPECT_FALSE(database.FindAsset(source->uuid));
}

TEST_F(GeneratedAssetCatalogTest, FailedMoveRollsBackFileAndRetainsIdentity)
{
    ASSERT_TRUE(database.PersistSourceIdentity(source->uuid));
    filesystem->fail = true;
    EXPECT_FALSE(database.MoveAsset(source->path, "renamed.model"));
    EXPECT_TRUE(filesystem->FileExist("character.model"));
    EXPECT_FALSE(filesystem->FileExist("renamed.model"));
    EXPECT_EQ(source->path, FilePath("character.model"));
    EXPECT_EQ(database.FindAsset(source->uuid).Get(), source.Get());
    filesystem->fail = false;
    EXPECT_TRUE(database.MoveAsset(source->path, "renamed.model"));
    EXPECT_EQ(source->path, FilePath("renamed.model"));
}

TEST_F(GeneratedAssetCatalogTest, RemovalRetainsWorkerManifestUpdatesFromIndependentCache)
{
    ASSERT_TRUE(filesystem->WriteFileExclusive("other.world", std::array<uint8_t, 1>{1}));
    const auto other = database.RegisterAsset("other.world", false);
    ASSERT_NE(other, nullptr);
    AssetDataBase worker;
    worker.SetWorkSpaceFs(filesystem);
    ASSERT_NE(worker.RegisterAsset(source->path, false), nullptr);
    GeneratedAssetManifest generated;
    ASSERT_TRUE(worker.UpdateGeneratedProducts(source->uuid, descriptors, defaults, generated));
    ASSERT_TRUE(worker.SetCookJson(source->uuid, R"({"targets":["worker"]})"));
    ASSERT_TRUE(database.RemoveAsset(other->uuid));
    ASSERT_TRUE(database.RefreshOwner(source->uuid));
    ASSERT_EQ(generated.records.size(), 2U);
    EXPECT_TRUE(database.Exists(generated.records.front().uuid));
    EXPECT_EQ(database.GetCookJson(source->uuid), R"({"targets":["worker"]})");
}

TEST_F(GeneratedAssetCatalogTest, MoveRetainsWorkerSourceAndDestinationManifestUpdates)
{
    ASSERT_TRUE(filesystem->WriteFileExclusive("moved/other.world", std::array<uint8_t, 1>{1}));
    const auto other = database.RegisterAsset("moved/other.world", false);
    ASSERT_NE(other, nullptr);
    AssetDataBase worker;
    worker.SetWorkSpaceFs(filesystem);
    ASSERT_NE(worker.RegisterAsset(source->path, false), nullptr);
    ASSERT_NE(worker.RegisterAsset(other->path, false), nullptr);
    GeneratedAssetManifest generated;
    ASSERT_TRUE(worker.UpdateGeneratedProducts(source->uuid, descriptors, defaults, generated));
    ASSERT_TRUE(worker.SetCookJson(source->uuid, R"({"targets":["source-worker"]})"));
    ASSERT_TRUE(worker.SetCookJson(other->uuid, R"({"targets":["destination-worker"]})"));
    ASSERT_TRUE(database.MoveAsset(source->path, "moved/character.model"));
    ASSERT_TRUE(database.RefreshOwner(source->uuid));
    EXPECT_TRUE(database.Exists(generated.records.front().uuid));
    EXPECT_EQ(database.GetCookJson(source->uuid), R"({"targets":["source-worker"]})");
    EXPECT_EQ(database.GetCookJson(other->uuid), R"({"targets":["destination-worker"]})");
}

TEST_F(GeneratedAssetCatalogTest, RegistrationRetainsWorkerManifestUpdatesAndDetectsBusyWriter)
{
    AssetDataBase worker;
    worker.SetWorkSpaceFs(filesystem);
    ASSERT_NE(worker.RegisterAsset(source->path, false), nullptr);
    GeneratedAssetManifest generated;
    ASSERT_TRUE(worker.UpdateGeneratedProducts(source->uuid, descriptors, defaults, generated));
    ASSERT_TRUE(filesystem->WriteFileExclusive("other.world", std::array<uint8_t, 1>{1}));
    FileLockPtr writer;
    ASSERT_TRUE(filesystem->AcquireFileLock("assets.jsonl.lock", writer));
    EXPECT_EQ(database.RegisterAsset("other.world", false), nullptr);
    EXPECT_EQ(database.RemoveAsset(source->uuid).status.error, FileOperationError::Busy);
    EXPECT_FALSE(database.MoveAsset(source->path, "moved/character.model"));
    writer.reset();
    ASSERT_NE(database.RegisterAsset("other.world", false), nullptr);
    ASSERT_TRUE(database.RefreshOwner(source->uuid));
    EXPECT_TRUE(database.Exists(generated.records.front().uuid));
}

TEST_F(GeneratedAssetCatalogTest, DeleteSeparatesFailedCommitFromCommittedCleanupFailure)
{
    const auto generated         = Generate();
    filesystem->failManifestOnce = "assets.jsonl";
    const auto rejected          = database.DeleteAsset(source->uuid);
    EXPECT_EQ(rejected.state, AssetCommitState::NotCommitted);
    EXPECT_TRUE(filesystem->FileExist(source->path));
    EXPECT_EQ(database.FindAsset(source->uuid).Get(), source.Get());
    ASSERT_TRUE(database.RefreshOwner(source->uuid));
    EXPECT_TRUE(database.Exists(generated.records.front().uuid));
    filesystem->failRemoval = true;
    const auto committed    = database.DeleteAsset(source->uuid);
    EXPECT_EQ(committed.state, AssetCommitState::CommittedNeedsRepair);
    EXPECT_FALSE(committed);
    ASSERT_EQ(committed.recovery.size(), 1U);
    EXPECT_EQ(committed.recovery.front().stage, "remove-staged-source");
    EXPECT_TRUE(committed.recovery.front().path.Exist());
    EXPECT_FALSE(filesystem->FileExist(source->path));
    EXPECT_FALSE(database.Exists(source->uuid));
    EXPECT_FALSE(database.Exists(generated.records.front().uuid));
    AssetDataBase reopened;
    reopened.SetWorkSpaceFs(filesystem);
    EXPECT_FALSE(reopened.RegisterAsset(source->path, false));
}

TEST_F(GeneratedAssetCatalogTest, CrossDirectoryMoveFailureRestoresFreshManifestSnapshotsUnderOrderedLocks)
{
    ASSERT_TRUE(filesystem->WriteFileExclusive("a/other.world", std::array<uint8_t, 1>{1}));
    const auto other = database.RegisterAsset("a/other.world", false);
    ASSERT_NE(other, nullptr);
    AssetDataBase worker;
    worker.SetWorkSpaceFs(filesystem);
    ASSERT_NE(worker.RegisterAsset(source->path, false), nullptr);
    ASSERT_NE(worker.RegisterAsset(other->path, false), nullptr);
    ASSERT_TRUE(worker.SetCookJson(other->uuid, R"({"targets":["worker"]})"));
    GeneratedAssetManifest generated;
    ASSERT_TRUE(worker.UpdateGeneratedProducts(source->uuid, descriptors, defaults, generated));
    filesystem->failManifestOnce = "assets.jsonl";
    filesystem->lockOrder.clear();
    EXPECT_FALSE(database.MoveAsset(source->path, "a/character.model"));
    EXPECT_EQ(filesystem->lockOrder, (std::vector<std::string>{"assets.jsonl.lock", "a/assets.jsonl.lock"}));
    EXPECT_TRUE(filesystem->FileExist(source->path));
    EXPECT_FALSE(filesystem->FileExist("a/character.model"));
    ASSERT_TRUE(database.RefreshOwner(source->uuid));
    EXPECT_TRUE(database.Exists(generated.records.front().uuid));
    EXPECT_EQ(database.GetCookJson(other->uuid), R"({"targets":["worker"]})");
    EXPECT_FALSE(database.FindAsset(FilePath("a/character.model")));
}

TEST_F(GeneratedAssetCatalogTest, UnreadableAndMalformedManifestRejectMutationsWithoutLosingIdentities)
{
    const auto  generated = Generate();
    std::string original;
    ASSERT_TRUE(filesystem->OpenFile("assets.jsonl")->ReadString(original));
    ASSERT_TRUE(filesystem->WriteFileExclusive("other.world", std::array<uint8_t, 1>{1}));
    filesystem->failRead = true;
    EXPECT_FALSE(database.RegisterAsset("other.world", false));
    EXPECT_FALSE(database.PersistSourceIdentity(source->uuid));
    EXPECT_FALSE(database.RemoveAsset(source->uuid));
    EXPECT_FALSE(database.DeleteAsset(source->uuid));
    EXPECT_FALSE(database.MoveAsset(source->path, "moved/character.model"));
    EXPECT_FALSE(database.SetCookJson(source->uuid, "{}"));
    GeneratedAssetManifest rejected;
    EXPECT_FALSE(database.UpdateGeneratedProducts(source->uuid, descriptors, defaults, rejected));
    filesystem->failRead = false;
    std::string current;
    ASSERT_TRUE(filesystem->OpenFile("assets.jsonl")->ReadString(current));
    EXPECT_EQ(current, original);
    EXPECT_TRUE(filesystem->FileExist(source->path));
    EXPECT_TRUE(database.Exists(generated.records.front().uuid));
    {
        std::ofstream output(root / "assets.jsonl", std::ios::app);
        output << "not-json\n";
    }
    EXPECT_FALSE(database.RegisterAsset("other.world", false));
    EXPECT_FALSE(database.RemoveAsset(source->uuid));
    EXPECT_FALSE(database.MoveAsset(source->path, "moved/character.model"));
    ASSERT_TRUE(filesystem->OpenFile("assets.jsonl")->ReadString(current));
    EXPECT_EQ(current, original + "not-json\n");
    EXPECT_TRUE(database.Exists(generated.records.front().uuid));
}

TEST_F(GeneratedAssetCatalogTest, FailedDeleteRenameCompensationReportsRecoverableBytes)
{
    const auto generated          = Generate();
    filesystem->failManifestOnce  = "assets.jsonl";
    filesystem->failRenameRestore = true;
    const auto result             = database.DeleteAsset(source->uuid);
    EXPECT_EQ(result.state, AssetCommitState::RollbackFailed);
    ASSERT_FALSE(result.recovery.empty());
    EXPECT_EQ(result.recovery.back().stage, "restore-source");
    EXPECT_TRUE(result.recovery.back().path.Exist());
    EXPECT_FALSE(database.FindAsset(source->uuid));
    EXPECT_FALSE(database.Exists(generated.records.front().uuid));
}

TEST_F(GeneratedAssetCatalogTest, FailedMoveCompensationNeverPublishesTheOldPath)
{
    const auto generated          = Generate();
    filesystem->failManifestOnce  = "assets.jsonl";
    filesystem->failRenameRestore = true;
    const auto result             = database.MoveAsset(source->path, "moved/character.model");
    EXPECT_EQ(result.state, AssetCommitState::RollbackFailed);
    EXPECT_FALSE(result.source);
    EXPECT_FALSE(database.FindAsset(source->uuid));
    EXPECT_FALSE(database.Exists(generated.records.front().uuid));
    EXPECT_TRUE(filesystem->FileExist("moved/character.model"));
    EXPECT_FALSE(filesystem->FileExist("character.model"));
    EXPECT_GE(result.recovery.size(), 2U);
}

TEST_F(GeneratedAssetCatalogTest, MissingManifestOwnerRejectsStaleRemovalAndMove)
{
    AssetDataBase worker;
    worker.SetWorkSpaceFs(filesystem);
    ASSERT_NE(worker.RegisterAsset(source->path, false), nullptr);
    ASSERT_TRUE(worker.RemoveAsset(source->uuid));
    EXPECT_FALSE(database.RemoveAsset(source->uuid));
    EXPECT_FALSE(database.DeleteAsset(source->uuid));
    EXPECT_FALSE(database.MoveAsset(source->path, "moved/character.model"));
    EXPECT_TRUE(filesystem->FileExist(source->path));
}

TEST_F(GeneratedAssetCatalogTest, CatalogConflictAfterCommitReportsRepairAndCanReplayManifest)
{
    GeneratedAssetManifest committed;
    filesystem->afterCommit = [&] {
        AssetIndexFile manifest("file", "cook");
        ASSERT_TRUE(AssetIndexFile::LoadChecked(filesystem, "assets.jsonl", manifest));
        const auto *entry = manifest.Find("character.model");
        ASSERT_NE(entry, nullptr);
        ASSERT_TRUE(ParseGeneratedAssets(source->uuid, entry->generated, committed));
        ASSERT_FALSE(committed.records.empty());
        AssetIndexFile conflict("file", "cook");
        conflict.Set({"conflict.world", committed.records.front().uuid, {}});
        ASSERT_TRUE(filesystem->WriteFileExclusive("other/conflict.world", std::array<uint8_t, 1>{1}));
        ASSERT_TRUE(AssetIndexFile::Save(filesystem, "other/assets.jsonl", conflict));
        ASSERT_NE(database.RegisterAsset("other/conflict.world", false), nullptr);
    };
    GeneratedAssetManifest output;
    const auto             result = database.UpdateGeneratedProducts(source->uuid, descriptors, defaults, output);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.state, AssetCommitState::CommittedNeedsRepair);
    EXPECT_EQ(result.stage, "publish-catalog");
    ASSERT_EQ(output.records.size(), 2U);
    EXPECT_EQ(output.records.front().uuid, committed.records.front().uuid);
    EXPECT_FALSE(database.RefreshOwner(source->uuid));
    ASSERT_TRUE(database.RemoveAsset(committed.records.front().uuid));
    EXPECT_TRUE(database.RefreshOwner(source->uuid));
    Uuid owner;
    EXPECT_TRUE(database.GetOwner(output.records.front().uuid, owner));
    EXPECT_EQ(owner, source->uuid);
}

TEST_F(GeneratedAssetCatalogTest, FailedDestinationManifestCompensationLeavesMoveExplicitlyQuarantined)
{
    Generate();
    filesystem->failManifestOnce       = "assets.jsonl";
    filesystem->failDestinationRestore = true;
    const auto result                  = database.MoveAsset(source->path, "moved/character.model");
    EXPECT_EQ(result.state, AssetCommitState::RollbackFailed);
    ASSERT_GE(result.recovery.size(), 2U);
    EXPECT_EQ(result.recovery.front().stage, "restore-destination-manifest");
    EXPECT_EQ(result.recovery.front().path, filesystem->GetPath() / FilePath("moved/assets.jsonl"));
    EXPECT_FALSE(database.FindAsset(source->uuid));
    EXPECT_FALSE(filesystem->FileExist("character.model"));
    EXPECT_TRUE(filesystem->FileExist("moved/character.model"));
    AssetIndexFile origin("file", "cook"), destination("file", "cook");
    ASSERT_TRUE(AssetIndexFile::LoadChecked(filesystem, "assets.jsonl", origin));
    ASSERT_TRUE(AssetIndexFile::LoadChecked(filesystem, "moved/assets.jsonl", destination));
    ASSERT_NE(origin.Find("character.model"), nullptr);
    ASSERT_NE(destination.Find("character.model"), nullptr);
    EXPECT_EQ(origin.Find("character.model")->id, destination.Find("character.model")->id);
}

TEST_F(GeneratedAssetCatalogTest, CapturedSourceFieldsAndFilesystemSurviveMutationAndRemoval)
{
    database.SetMarkedName(source->uuid, "original");
    const auto dependency = Uuid::Create();
    database.SetBuildDependencies(source->uuid, {dependency});
    const auto snapshot = database.CaptureSource(source->uuid);
    ASSERT_TRUE(snapshot.source);
    ASSERT_NE(snapshot.source.Get(), source.Get());
    EXPECT_TRUE(database.MoveAsset(source->path, "moved/character.model"));
    database.SetMarkedName(source->uuid, "changed");
    database.SetBuildDependencies(source->uuid, {});
    EXPECT_TRUE(database.RemoveAsset(source->uuid));
    database.SetWorkSpaceFs({});
    EXPECT_FALSE(database.CaptureSource(source->uuid).source);
    EXPECT_EQ(snapshot.source->path, FilePath("character.model"));
    EXPECT_EQ(snapshot.source->name, "original");
    EXPECT_EQ(snapshot.source->dependencies, std::vector<Uuid>{dependency});
    EXPECT_EQ(snapshot.filesystem.Get(), filesystem.Get());
    EXPECT_TRUE(snapshot.filesystem->FileExist("moved/character.model"));
}

TEST_F(GeneratedAssetCatalogTest, RegistrationOpensCookInputWithoutHoldingTheAssetMutex)
{
    std::promise<bool> observed;
    auto               ready = observed.get_future();
    std::thread        reader;
    bool               memoryAvailable = false;
    filesystem->beforeSourceOpen       = [&] {
        reader          = std::thread([&] { observed.set_value(database.CaptureSource(source->uuid).source != nullptr); });
        memoryAvailable = ready.wait_for(std::chrono::seconds(1)) == std::future_status::ready;
    };
    const auto registered = database.RegisterAsset(source->path, true);
    if (reader.joinable())
        reader.join();
    AssetExecutor::Get()->WaitForAll();
    ASSERT_NE(registered, nullptr);
    EXPECT_TRUE(memoryAvailable);
    ASSERT_EQ(ready.wait_for(std::chrono::seconds(0)), std::future_status::ready);
    EXPECT_TRUE(ready.get());
}

TEST_F(GeneratedAssetCatalogTest, ManifestIOLeavesPublishedMemoryReadable)
{
    std::promise<void> entered, release;
    auto               gate    = release.get_future().share();
    auto               started = entered.get_future();
    filesystem->afterCommit    = [&] {
        entered.set_value();
        gate.wait();
    };
    auto       write   = std::async(std::launch::async, [&] { return database.SetCookJson(source->uuid, R"({"targets":["updated"]})"); });
    const bool writing = started.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    EXPECT_TRUE(writing);
    auto       read     = std::async(std::launch::async, [&] { return database.CaptureSource(source->uuid); });
    const bool readable = read.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready;
    release.set_value();
    EXPECT_TRUE(readable);
    EXPECT_TRUE(read.get().source);
    EXPECT_TRUE(write.get());
}

TEST_F(GeneratedAssetCatalogTest, ResetCancelsAWaitingManifestWriter)
{
    FileLockPtr held;
    ASSERT_TRUE(filesystem->AcquireFileLock("assets.jsonl.lock", held));
    auto write = std::async(std::launch::async, [&] { return database.SetCookJson(source->uuid, "{}"); });
    // The bounded retry also makes teardown safe if Reset wins before the writer starts.
    auto reset = std::async(std::launch::async, [&] { database.Reset(); });
    EXPECT_EQ(reset.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    reset.get();
    const auto result = write.get();
    EXPECT_FALSE(result);
    EXPECT_TRUE(result.status.error == FileOperationError::Busy || result.status.error == FileOperationError::InvalidPath);
    EXPECT_FALSE(database.FindAsset(source->uuid));
}

TEST_F(GeneratedAssetCatalogTest, CapturedCookBytesRemainOwnedAndStaleInputsAreRejected)
{
    AssetBuildRequest request;
    ASSERT_TRUE(database.PrepareBuildInput(source->uuid, request));
    std::vector<uint8_t> bytes;
    ASSERT_TRUE(request.file->ReadBin(bytes));
    EXPECT_EQ(bytes, (std::vector<uint8_t>{1}));
    std::unique_ptr<AssetPublicationScope> scope;
    ASSERT_TRUE(request.publication->Acquire(scope));
    scope.reset();
    ASSERT_TRUE(database.WriteSource(source->uuid, "changed"));
    ASSERT_TRUE(request.file->ReadBin(bytes));
    EXPECT_EQ(bytes, (std::vector<uint8_t>{1}));
    EXPECT_EQ(request.publication->Acquire(scope).error, ProductError::Stale);
    EXPECT_FALSE(scope);
}

TEST_F(GeneratedAssetCatalogTest, CookGuardRejectsSettingsChangesButAllowsGeneratedCatalogUpdates)
{
    AssetBuildRequest request;
    ASSERT_TRUE(database.PrepareBuildInput(source->uuid, request));
    Generate();
    std::unique_ptr<AssetPublicationScope> scope;
    ASSERT_TRUE(request.publication->Acquire(scope));
    scope.reset();
    ASSERT_TRUE(database.SetCookJson(source->uuid, R"({"settings":{"common":{"quality":2}}})"));
    EXPECT_EQ(request.publication->Acquire(scope).error, ProductError::Stale);
}

TEST_F(GeneratedAssetCatalogTest, CookGuardRejectsExternalSourceEditsAndSourceReincarnation)
{
    AssetBuildRequest request;
    ASSERT_TRUE(database.PrepareBuildInput(source->uuid, request));
    {
        std::ofstream stream(root / "character.model", std::ios::binary | std::ios::trunc);
        stream << "external edit";
    }
    std::unique_ptr<AssetPublicationScope> scope;
    EXPECT_EQ(request.publication->Acquire(scope).error, ProductError::Stale);
    ASSERT_TRUE(database.PrepareBuildInput(source->uuid, request));
    ASSERT_TRUE(database.DeleteAsset(source->uuid));
    EXPECT_EQ(request.publication->Acquire(scope).error, ProductError::Stale);
}

TEST_F(GeneratedAssetCatalogTest, GeneratedIdentityCommitRejectsAnOutdatedCookInput)
{
    AssetBuildRequest request;
    ASSERT_TRUE(database.PrepareBuildInput(source->uuid, request));
    ASSERT_TRUE(database.WriteSource(source->uuid, "new source revision"));
    GeneratedAssetManifest rejected;
    EXPECT_EQ(database.UpdateGeneratedProducts(source->uuid, descriptors, defaults, rejected, request.publication).error, ProductError::Stale);
    std::vector<GeneratedAssetRecord> children;
    database.ForEachGenerated([&](const GeneratedAssetRecord &value) { children.push_back(value); });
    EXPECT_TRUE(children.empty());
}

TEST_F(GeneratedAssetCatalogTest, ConcurrentWorkerPublicationAndWarmScanPreserveGeneratedIdentities)
{
    ASSERT_TRUE(database.MoveAsset(source->path, "character.world"));
    AssetDataBase worker;
    worker.SetWorkSpaceFs(new NativeFileSystem(FilePath(root)));
    ASSERT_TRUE(worker.RegisterAsset(source->path, false));
    std::promise<void> start;
    const auto         gate    = start.get_future().share();
    auto               writing = std::async(std::launch::async, [&] {
        gate.wait();
        for (size_t i = 0; i < 32; ++i) {
            GeneratedAssetManifest output;
            if (!worker.UpdateGeneratedProducts(source->uuid, descriptors, defaults, output))
                return false;
        }
        return true;
    });
    start.set_value();
    for (size_t i = 0; i < 32; ++i)
        EXPECT_TRUE(database.RebuildCacheFromScan().Complete());
    EXPECT_TRUE(writing.get());
    ASSERT_TRUE(database.RefreshOwner(source->uuid));
    std::map<std::string, Uuid> resolved;
    ASSERT_TRUE(database.GetGeneratedDefaults(source->uuid, resolved));
    EXPECT_EQ(resolved.size(), defaults.size());
    for (const auto &[role, id] : resolved)
        EXPECT_TRUE(database.Exists(id));
}

TEST_F(GeneratedAssetCatalogTest, StaleCookCannotPublishDependenciesIntoNewSourceRevision)
{
    AssetBuildRequest request;
    ASSERT_TRUE(database.PrepareBuildInput(source->uuid, request));
    const auto dependency           = Uuid::Create();
    request.assetInfo->dependencies = {dependency};
    ASSERT_TRUE(database.SetBuildDependencies(request));
    ASSERT_TRUE(database.WriteSource(source->uuid, "changed"));
    request.assetInfo->dependencies.clear();
    EXPECT_FALSE(database.SetBuildDependencies(request));
    EXPECT_EQ(database.CaptureSource(source->uuid).source->dependencies, std::vector<Uuid>{dependency});
}

TEST(GeneratedAssetCatalog, DifferentInputVersionsDoNotCoalesceAndFailuresCompleteEverySubscriber)
{
    class VersionedBuilder final : public AssetBuilder {
    public:
        std::promise<void>    started, release;
        std::atomic<uint32_t> requests{0};
        void                  Request(const AssetBuildRequest &request, AssetBuildResult &result) override
        {
            ++requests;
            if (request.localRevision == "old") {
                started.set_value();
                release.get_future().wait();
                result.retCode = AssetBuildRetCode::FAILED;
                result.error   = "outdated input";
            } else {
                result.retCode = AssetBuildRetCode::SUCCESS;
            }
        }
        const std::vector<std::string> &GetExtensions() const override
        {
            static const std::vector<std::string> extensions{".versioned-test"};
            return extensions;
        }
    };
    AssetBuilderManager manager;
    auto               *builder = new VersionedBuilder;
    auto                started = builder->started.get_future();
    manager.RegisterBuilder(builder);
    AssetBuildRequest request;
    request.assetInfo       = new AssetSourceInfo;
    request.assetInfo->uuid = Uuid::Create();
    request.assetInfo->ext  = ".versioned-test";
    request.localRevision   = "old";
    std::promise<AssetBuildRetCode> first, second, newer;
    auto                            a = first.get_future(), b = second.get_future(), c = newer.get_future();
    manager.BuildRequest(request, [&](const AssetBuildResult &r) { first.set_value(r.retCode); });
    EXPECT_EQ(started.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    manager.BuildRequest(request, [&](const AssetBuildResult &r) { second.set_value(r.retCode); });
    request.localRevision = "new";
    manager.BuildRequest(request, [&](const AssetBuildResult &r) { newer.set_value(r.retCode); });
    builder->release.set_value();
    AssetExecutor::Get()->WaitForAll();
    ASSERT_EQ(a.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    ASSERT_EQ(b.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    ASSERT_EQ(c.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    EXPECT_EQ(a.get(), AssetBuildRetCode::FAILED);
    EXPECT_EQ(b.get(), AssetBuildRetCode::FAILED);
    EXPECT_EQ(c.get(), AssetBuildRetCode::SUCCESS);
    EXPECT_EQ(builder->requests, 2u);
}
