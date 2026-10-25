#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetDependencyProvider.h>
#include <framework/asset/AssetExecutor.h>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>

using namespace sky;

namespace {
    class ManifestFileSystem : public NativeFileSystem {
    public:
        using NativeFileSystem::NativeFileSystem;
        bool                fail = false;
        FileOperationStatus ReplaceFileAtomic(const FilePath &from, const FilePath &to) const override
        {
            return fail ? FileOperationStatus{FileOperationError::IOError, "injected manifest failure"}
                        : NativeFileSystem::ReplaceFileAtomic(from, to);
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
    ASSERT_NE(database.MoveAsset("character.model", "moved/character.model"), nullptr);
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

TEST_F(GeneratedAssetCatalogTest, VerifiedLegacyIdentityIsPersistentAndCannotStealOrRebindAnExistingIdentity)
{
    const auto legacy = Uuid::Create();
    descriptors[1].verifiedLegacyId = legacy;
    const auto first = Generate();
    ASSERT_EQ(first.records.size(), 2U);
    EXPECT_EQ(first.records[1].uuid, legacy);
    descriptors[1].verifiedLegacyId = {};
    EXPECT_EQ(Generate().records[1].uuid, legacy);
    GeneratedAssetManifest result;
    descriptors[1].verifiedLegacyId = Uuid::Create();
    EXPECT_FALSE(database.UpdateGeneratedProducts(source->uuid, descriptors, defaults, result));
    EXPECT_TRUE(database.Exists(legacy));
    descriptors[1].key = "other-rig";
    defaults["skeleton"] = "other-rig";
    EXPECT_FALSE(database.UpdateGeneratedProducts(source->uuid, descriptors, defaults, result));
    ASSERT_TRUE(filesystem->WriteFileExclusive("other.model", std::array<uint8_t, 1>{1}));
    const auto other = database.RegisterAsset("other.model", false);
    ASSERT_TRUE(other);
    descriptors[1].verifiedLegacyId = legacy;
    EXPECT_EQ(database.UpdateGeneratedProducts(other->uuid, descriptors, defaults, result).error, ProductError::OwnerConflict);
    descriptors[1].verifiedLegacyId = source->uuid;
    EXPECT_EQ(database.UpdateGeneratedProducts(other->uuid, descriptors, defaults, result).error, ProductError::OwnerConflict);
}

TEST_F(GeneratedAssetCatalogTest, DuplicateGetsNewChildIdsAndRemovalKeepsTombstone)
{
    const auto original = Generate();
    ASSERT_EQ(original.records.size(), 2U);
    const auto copy = database.DuplicateAsset(source->path, "copy.model");
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy->uuid, source->uuid);
    std::map<std::string, Uuid> copied;
    ASSERT_TRUE(database.GetGeneratedDefaults(copy->uuid, copied));
    EXPECT_NE(copied["skin"], original.records[0].uuid);
    EXPECT_NE(copied["skeleton"], original.records[1].uuid);
    EXPECT_EQ(copied["mesh"], copy->uuid);
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
    database.RemoveAsset(copy->uuid);
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
