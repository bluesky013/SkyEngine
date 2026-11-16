//
// Created by blues on 2024/6/21.
//

#include <framework/asset/AssetBuilder.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <gtest/gtest.h>

#include <core/hash/Hash.h>
#include <framework/asset/AssetIndexFile.h>
#include <framework/asset/AssetProductBundle.h>
#include <framework/asset/CookConfig.h>
#include <framework/asset/CookWorker.h>
#include <framework/asset/ISourceCatalog.h>
#include <framework/asset/InProcessCookRunner.h>
#include <framework/compression/Compressor.h>
#include <framework/platform/PlatformBase.h>
#include <framework/serialization/CoreReflection.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/WorldAsset.h>
#include <test/EngineRoot.h>

#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <thread>

using namespace sky;

TEST(AssetManagerBuiltins, RecreatedManagerProvidesWorldWithoutModuleRegistration)
{
    for (uint32_t iteration = 0; iteration < 2; ++iteration) {
        AssetManager manager;
        const auto   id    = Uuid::Create();
        const auto   asset = manager.CreateBuildAsset(id, Name(AssetTraits<World>::ASSET_TYPE.data()));
        ASSERT_NE(asset, nullptr);
        EXPECT_EQ(asset->GetUuid(), id);
        ASSERT_EQ(asset->GetType(), Name("World"));
        const auto typed = std::static_pointer_cast<Asset<World>>(asset);
        EXPECT_TRUE(typed->Data().IsLoadValid());
        EXPECT_TRUE(typed->Data().actors.empty());
    }
}

TEST(AssetProductPaths, ProductIndexesRejectConflictingLogicalIdentities)
{
    class RecordingCatalog final : public ISourceCatalog {
    public:
        bool ResolvePath(const std::string &path, Uuid &out) const override
        {
            paths.push_back(path);
            if (path == "materials/uncooked.mat") {
                out = sourceId;
                return true;
            }
            return false;
        }
        bool Exists(const Uuid &) const override
        {
            return false;
        }
        bool GetType(const Uuid &, std::string &) const override
        {
            return false;
        }
        bool GetTarget(const Uuid &, std::string &) const override
        {
            return false;
        }
        bool GetSourcePath(const Uuid &, std::string &) const override
        {
            return false;
        }
        Uuid                             sourceId = Uuid::Create();
        mutable std::vector<std::string> paths;
    } catalog;
    namespace fs = std::filesystem;
    std::error_code error;
    const auto      temporary = fs::temp_directory_path(error);
    ASSERT_FALSE(error);
    struct Cleanup {
        fs::path root;
        ~Cleanup()
        {
            std::error_code error;
            fs::remove_all(root, error);
            EXPECT_FALSE(error);
        }
    } cleanup{temporary / ("sky-product-paths-" + Uuid::Create().ToString())};
    const Uuid                     firstId = Uuid::Create(), conflictingId = Uuid::Create();
    const std::vector<std::string> invalidPaths{"",
                                                ".",
                                                "./",
                                                "..",
                                                "worlds/..",
                                                "../worlds/main.world",
                                                "worlds/../../main.world",
                                                "/worlds/main.world",
                                                "\\worlds\\main.world",
                                                "C:\\worlds\\main.world",
                                                "worlds:main.world",
                                                "//server/worlds/main.world",
                                                std::string("worlds/main.world") + '\0' + "hidden"};
    const auto                     bundle = [&](const char *name, const Uuid &id) {
        const FileSystemPtr filesystem = new NativeFileSystem(FilePath(cleanup.root / name));
        AssetIndexFile      index("path");
        index.Set({"worlds/main.world", id, {}});
        for (const auto &path : invalidPaths)
            index.Set({path, id, {}});
        EXPECT_TRUE(AssetIndexFile::Save(filesystem, FilePath("product.index"), index));
        return new PackageAssetBundle(filesystem, name);
    };
    AssetManager manager;
    manager.SetSourceCatalog(&catalog);
    EXPECT_EQ(manager.ResolveAssetPath("./materials/uncooked.mat"), catalog.sourceId);
    EXPECT_FALSE(manager.ResolveAssetPath("materials/uncooked.mat", AssetLoadPolicy::ProductsOnly));
    EXPECT_EQ(catalog.paths, (std::vector<std::string>{"materials/uncooked.mat"}));
    catalog.paths.clear();
    manager.AddAssetProductBundle(bundle("first", firstId));
    EXPECT_EQ(manager.ResolveProductPath("worlds/main.world"), firstId);
    EXPECT_EQ(manager.ResolveProductPath("worlds\\main.world"), firstId);
    EXPECT_EQ(manager.ResolveProductPath("./worlds\\main.world"), firstId);
    EXPECT_EQ(manager.ResolveProductPath("scratch/../worlds/./main.world"), firstId);
    EXPECT_EQ(manager.ResolveProductPath("worlds//nested/../main.world/"), firstId);
    EXPECT_FALSE(manager.ResolveProductPath("worlds/missing.world"));
    EXPECT_EQ(manager.LoadAssetFromPath("./worlds\\draft\\..\\missing.world", AssetLoadPolicy::AllowCook), nullptr);
    EXPECT_EQ(catalog.paths, (std::vector<std::string>{"worlds/missing.world"}));
    EXPECT_EQ(manager.LoadAssetFromPath("./worlds\\missing.world", AssetLoadPolicy::ProductsOnly), nullptr);
    for (const auto &path : invalidPaths) {
        SCOPED_TRACE(path);
        EXPECT_FALSE(manager.ResolveProductPath(path));
        EXPECT_EQ(manager.LoadAssetFromPath(path, AssetLoadPolicy::AllowCook), nullptr);
        EXPECT_EQ(manager.LoadAssetFromPath(path, AssetLoadPolicy::ProductsOnly), nullptr);
    }
    EXPECT_EQ(catalog.paths, (std::vector<std::string>{"worlds/missing.world"}));
    manager.AddAssetProductBundle(bundle("same", firstId));
    EXPECT_EQ(manager.ResolveProductPath("worlds/main.world"), firstId);
    manager.AddAssetProductBundle(bundle("conflict", conflictingId));
    EXPECT_FALSE(manager.ResolveProductPath("worlds/main.world"));
    EXPECT_FALSE(manager.ResolveAssetPath("worlds/main.world"));
    EXPECT_EQ(manager.LoadAssetFromPath("worlds/main.world", AssetLoadPolicy::ProductsOnly), nullptr);
}

namespace {
    class AssetScanTest : public testing::Test {
    protected:
        void SetUp() override
        {
            std::error_code error;
            const auto      temp = std::filesystem::temp_directory_path(error);
            ASSERT_FALSE(error);
            root      = temp / ("sky-asset-scan-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            workspace = root / "project" / "assets";
            engine    = root / "engine" / "assets";
            database.SetWorkSpaceFs(new NativeFileSystem(FilePath(workspace)));
            database.SetEngineFs(new NativeFileSystem(FilePath(engine.parent_path())));
            std::ofstream(workspace / "scene.world") << "{}";
        }
        void TearDown() override
        {
            if (!root.empty()) {
                std::error_code error;
                std::filesystem::remove_all(root, error);
                EXPECT_FALSE(error);
            }
        }
        AssetDataBase         database;
        std::filesystem::path root, workspace, engine;
    };

    TEST_F(AssetScanTest, FailedMountDoesNotPublishEarlierMountAndRetrySucceeds)
    {
        std::error_code error;
        ASSERT_TRUE(std::filesystem::remove(engine, error));
        ASSERT_FALSE(error);
        const auto failure = database.RebuildCacheFromScan();
        ASSERT_FALSE(failure.Succeeded());
        EXPECT_EQ(failure.errors.front().path, FilePath(engine));
        EXPECT_EQ(failure.errors.front().code, std::errc::no_such_file_or_directory);
        EXPECT_TRUE(database.GetSources().empty());
        EXPECT_FALSE(std::filesystem::exists(workspace / "assets.jsonl", error));
        ASSERT_TRUE(std::filesystem::create_directory(engine, error));
        ASSERT_FALSE(error);
        EXPECT_TRUE(database.RebuildCacheFromScan().Succeeded());
        ASSERT_EQ(database.GetSources().size(), 1u);
        EXPECT_NE(database.FindAsset(FilePath("scene.world")), nullptr);
    }

    TEST_F(AssetScanTest, FailedLoadDoesNotImportCacheAndRetryKeepsCachedIdentity)
    {
        const auto original = database.RegisterAsset("scene.world", false);
        ASSERT_NE(original, nullptr);
        const auto id = original->uuid;
        database.Save();
        database.Reset();
        std::error_code error;
        ASSERT_TRUE(std::filesystem::remove(engine, error));
        ASSERT_FALSE(error);
        const auto failure = database.Load();
        ASSERT_FALSE(failure.Succeeded());
        EXPECT_EQ(failure.errors.front().path, FilePath(engine));
        EXPECT_TRUE(database.GetSources().empty());
        ASSERT_TRUE(std::filesystem::create_directory(engine, error));
        ASSERT_FALSE(error);
        EXPECT_TRUE(database.Load().Succeeded());
        const auto restored = database.FindAsset(FilePath("scene.world"));
        ASSERT_NE(restored, nullptr);
        EXPECT_EQ(restored->uuid, id);
    }

    TEST_F(AssetScanTest, FailedMigrationRestoresOrdinaryReadOnlyMountBehavior)
    {
        std::error_code error;
        ASSERT_TRUE(std::filesystem::remove(engine, error));
        ASSERT_FALSE(error);
        EXPECT_FALSE(database.MigrateLegacyIdentity().Succeeded());
        EXPECT_TRUE(database.GetSources().empty());
        ASSERT_TRUE(std::filesystem::create_directory(engine, error));
        ASSERT_FALSE(error);
        std::ofstream(engine / "engine.world") << "{}";
        EXPECT_TRUE(database.RebuildCacheFromScan().Succeeded());
        EXPECT_NE(database.FindAsset(FilePath("engine.world")), nullptr);
        EXPECT_FALSE(std::filesystem::exists(engine / "assets.jsonl", error));
        EXPECT_FALSE(error);
    }
} // namespace

// Identity override isolates product framing from the built-in codec.
class DummyCompressor : public ICompressor {
public:
    uint32_t CompressBound(uint32_t inDataSize) override
    {
        return inDataSize;
    }
    CompressResult Compress(const std::span<const uint8_t> &inData, const std::span<uint8_t> &out, uint32_t) override
    {
        if (out.size() < inData.size()) {
            return {false, 0};
        }
        std::memcpy(out.data(), inData.data(), inData.size());
        return {true, static_cast<uint32_t>(inData.size())};
    }
    CompressResult DeCompress(const std::span<const uint8_t> &inData, const std::span<uint8_t> &out, uint32_t) override
    {
        if (out.size() < inData.size()) {
            return {false, 0};
        }
        std::memcpy(out.data(), inData.data(), inData.size());
        return {true, static_cast<uint32_t>(inData.size())};
    }
};

struct MAggData : public RefObject {
    int v = 0;
};

template <>
struct AssetTraits<MAggData> {
    using DataType                                = MAggData;
    static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;

    static constexpr std::string_view ASSET_TYPE = "MAgg";
};

// Saves one product per requested target so multi-platform output can be verified.
class MultiTargetBuilder : public AssetBuilder {
public:
    void Request(const AssetBuildRequest &request, AssetBuildResult &result) override
    {
        auto             archive = request.file->ReadAsArchive();
        JsonInputArchive json(*archive);
        json.Start("val");
        const int v = json.LoadInt();
        json.End();

        auto *am        = AssetManager::Get();
        auto  asset     = std::static_pointer_cast<Asset<MAggData>>(am->CreateBuildAsset(request.assetInfo->uuid, Name("MAgg")));
        asset->Data().v = v;
        if (const auto status = am->SaveAsset(asset, request.target); !status) {
            result.retCode = AssetBuildRetCode::FAILED;
            result.error   = status.message;
            return;
        }
        result.retCode = AssetBuildRetCode::SUCCESS;
    }

    const std::vector<std::string> &GetExtensions() const override
    {
        static std::vector<std::string> ext = {".mt"};
        return ext;
    }

    std::string_view QueryType(const std::string &) const override
    {
        return AssetTraits<MAggData>::ASSET_TYPE;
    }
};

// Reports a fixed settings description so AssetBuilderManager::GetBuilderSettings can be exercised.
class SettingsBuilder : public AssetBuilder {
public:
    const std::vector<std::string> &GetExtensions() const override
    {
        static std::vector<std::string> ext = {".setup"};
        return ext;
    }

    std::vector<std::pair<std::string, std::string>> DescribeSettings(const ProductBundleKey &) const override
    {
        return {{"encode", "BC7"}, {"srgb", "true"}};
    }
};

struct T1Data : public RefObject {
    int v;
};

struct T2Data : public RefObject {
    float v;
    int   extVal;
};

template <>
struct AssetTraits<T1Data> {
    using DataType                                = T1Data;
    static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;

    static constexpr std::string_view ASSET_TYPE = "T1";
};

template <>
struct AssetTraits<T2Data> {
    using DataType                                = T2Data;
    static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;

    static constexpr std::string_view ASSET_TYPE = "T2";
};

struct T3Data : public RefObject {
    Uuid t1;
    Uuid t2;
};

template <>
struct AssetTraits<T3Data> {
    using DataType                                = T3Data;
    static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;

    static constexpr std::string_view ASSET_TYPE = "T3";
};

// Header + bulk asset: the header carries a byte range into the product bulk.
struct BulkData : public RefObject {
    uint32_t magic  = 0;
    uint64_t offset = 0;
    uint64_t size   = 0;
};

template <>
struct AssetTraits<BulkData> {
    using DataType                                = BulkData;
    static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;

    static constexpr std::string_view ASSET_TYPE = "Bulk";
};

class TestBuilder1 : public AssetBuilder {
public:
    TestBuilder1()           = default;
    ~TestBuilder1() override = default;

    struct Config {
        int extVal = 0;
    };

    const std::vector<std::string> &GetExtensions() const override
    {
        static std::vector<std::string> ext = {".t1", ".t2"};
        return ext;
    }

    std::string GetDefaultBundle() const
    {
#if WIN32
        auto type = PlatformType::Windows;
#else
        auto type = PlatformType::MacOS;
#endif
        auto iter = defaultBundles.find(type);
        SKY_ASSERT(iter != defaultBundles.end());
        return iter->second;
    }

    void LoadConfig(const FileSystemPtr &cfg) override
    {
        auto             archive = cfg->OpenFile("asset_cfg_t2.json")->ReadAsArchive();
        JsonInputArchive json(*archive);

        json.Start("t2");

        json.ForEachMember([this, &json](const std::string &key) {
            auto &cfg = configs[key];
            json.Start(key);

            json.Start("extVal");
            cfg.extVal = json.LoadInt();
            json.End();

            json.End();
        });

        json.End();

        json.Start("defaultBundles");

        json.ForEachMember([&json, this](const std::string &key) {
            json.Start(key);
            auto platformType = Platform::GetPlatformTypeByName(key);
            if (platformType != PlatformType::UNDEFINED) {
                defaultBundles[platformType] = json.LoadString();
            }
            json.End();
        });

        json.End();
    }

    void Request(const AssetBuildRequest &request, AssetBuildResult &result) override
    {
        auto             archive = request.file->ReadAsArchive();
        JsonInputArchive json(*archive);

        auto *am      = AssetManager::Get();
        auto *builder = AssetBuilderManager::Get()->QueryBuilder(request.assetInfo->ext);
        if (builder != nullptr && builder->QueryType(request.assetInfo->ext) == AssetTraits<T1Data>::ASSET_TYPE) {
            auto asset = std::static_pointer_cast<Asset<T1Data>>(am->CreateBuildAsset(request.assetInfo->uuid, Name("T1")));

            T1Data &data = asset->Data();
            json.Start("val");
            data.v = json.LoadInt();

            if (const auto status = am->SaveAsset(asset, "common"); !status) {
                result.retCode = AssetBuildRetCode::FAILED;
                result.error   = status.message;
                return;
            }
            result.retCode = AssetBuildRetCode::SUCCESS;
        } else {
            auto asset = std::static_pointer_cast<Asset<T2Data>>(am->CreateBuildAsset(request.assetInfo->uuid, Name("T2")));

            T2Data &data = asset->Data();
            json.Start("val");
            data.v = static_cast<float>(json.LoadDouble());

            auto target = request.target.empty() ? GetDefaultBundle() : request.target;

            auto iter = configs.find(target);
            if (iter != configs.end()) {
                data.extVal = iter->second.extVal;
                if (const auto status = am->SaveAsset(asset, target); !status) {
                    result.retCode = AssetBuildRetCode::FAILED;
                    result.error   = status.message;
                    return;
                }
                result.retCode = AssetBuildRetCode::SUCCESS;
            }
        }
    }

    std::string_view QueryType(const std::string &ext) const override
    {
        return ext == ".t1" ? AssetTraits<T1Data>::ASSET_TYPE : AssetTraits<T2Data>::ASSET_TYPE;
    }

private:
    std::unordered_map<std::string, Config>       configs;
    std::unordered_map<PlatformType, std::string> defaultBundles;
};

class TestBuilder2 : public AssetBuilder {
public:
    TestBuilder2()           = default;
    ~TestBuilder2() override = default;

    void Request(const AssetBuildRequest &request, AssetBuildResult &result) override
    {
        auto             archive = request.file->ReadAsArchive();
        JsonInputArchive json(*archive);

        json.Start("v1");
        std::string p1 = json.LoadString();
        json.End();

        json.Start("v2");
        std::string p2 = json.LoadString();
        json.End();

        auto *am    = AssetManager::Get();
        auto  asset = std::static_pointer_cast<Asset<T3Data>>(am->CreateBuildAsset(request.assetInfo->uuid, Name("T3")));

        auto p1Asset = AssetDataBase::Get()->RegisterAsset(p1);
        auto p2Asset = AssetDataBase::Get()->RegisterAsset(p2);
        ASSERT_NE(p1Asset, nullptr);
        ASSERT_NE(p2Asset, nullptr);

        request.assetInfo->dependencies.emplace_back(p1Asset->uuid);
        request.assetInfo->dependencies.emplace_back(p2Asset->uuid);

        auto &data = asset->Data();
        data.t1    = p1Asset->uuid;
        data.t2    = p2Asset->uuid;

        asset->AddDependencies(p1Asset->uuid);
        asset->AddDependencies(p2Asset->uuid);

        if (const auto status = am->SaveAsset(asset, "common"); !status) {
            result.retCode = AssetBuildRetCode::FAILED;
            result.error   = status.message;
            return;
        }
        result.retCode = AssetBuildRetCode::SUCCESS;
    }

    const std::vector<std::string> &GetExtensions() const override
    {
        static std::vector<std::string> ext = {".t3"};
        return ext;
    }

    std::string_view QueryType(const std::string &ext) const override
    {
        return AssetTraits<T3Data>::ASSET_TYPE;
    }
};

class AssetManagerTest : public ::testing::Test {
public:
    static void SetUpTestSuite()
    {
        auto *context = SerializationContext::Get();

        context->Register<T1Data>("T1Data").Member<&T1Data::v>("v");

        context->Register<T2Data>("T2Data").Member<&T2Data::v>("v").Member<&T2Data::extVal>("extVal");

        context->Register<T3Data>("T3Data").Member<&T3Data::t1>("t1").Member<&T3Data::t2>("t2");

        context->Register<BulkData>("BulkData").Member<&BulkData::magic>("magic").Member<&BulkData::offset>("offset").Member<&BulkData::size>("size");

        NativeFileSystemPtr projectFs = new NativeFileSystem(PROJECT_ROOT);
        AssetDataBase::Get()->SetEngineFs(new NativeFileSystem(ENGINE_ROOT));
        AssetDataBase::Get()->SetWorkSpaceFs(projectFs->CreateSubSystem("assets", true));
        AssetManager::Get()->SetSourceCatalog(AssetDataBase::Get());

        AssetBuilderManager::Get()->RegisterBuilder(new TestBuilder1());
        AssetBuilderManager::Get()->RegisterBuilder(new TestBuilder2());

        ASSERT_TRUE(AssetBuilderManager::Get()->SetWorkSpaceFs(projectFs));
        AssetBuilderManager::Get()->LoadBuildConfigs(projectFs->CreateSubSystem("configs", false));

        AssetManager::Get()->RegisterAssetHandler<T1Data>();
        AssetManager::Get()->RegisterAssetHandler<T2Data>();
        AssetManager::Get()->RegisterAssetHandler<T3Data>();
        AssetManager::Get()->RegisterAssetHandler<BulkData>();
    }

    static void TearDownTestSuite()
    {
    }
};

TEST_F(AssetManagerTest, BuilderTest)
{
    auto *db = AssetDataBase::Get();
    EXPECT_TRUE(db->Load().Succeeded());
    auto src = db->RegisterAsset("framework/data/test_asset.t3");
    db->Save();
    db->Dump(std::cout);

    auto asset = AssetManager::Get()->LoadAsset<T3Data>(src->uuid);
    ASSERT_NE(asset, nullptr);
    asset->BlockUntilLoaded();

    auto &data = asset->Data();
    auto  t1   = AssetManager::Get()->FindAsset<T1Data>(data.t1);
    auto  t2   = AssetManager::Get()->FindAsset<T2Data>(data.t2);

    ASSERT_EQ(t1->Data().v, 1);
    ASSERT_EQ(t2->Data().v, 2.f);
#if WIN32
    ASSERT_EQ(t2->Data().extVal, 3);
#else
    ASSERT_EQ(t2->Data().extVal, 4);
#endif

    // Path resolution goes through the product index (canonical logical path).
    auto byPath = AssetManager::Get()->LoadAssetFromPath("framework/data/test_asset.t3");
    ASSERT_NE(byPath, nullptr);
    byPath->BlockUntilLoaded();
    EXPECT_TRUE(byPath->IsLoaded());
}

TEST_F(AssetManagerTest, MountProvenanceTest)
{
    namespace fs = std::filesystem;

    const fs::path engRoot  = "mount_engine_tmp";
    const fs::path projRoot = "mount_proj_tmp";
    fs::remove_all(engRoot);
    fs::remove_all(projRoot);
    fs::create_directories(engRoot / "assets" / "framework" / "data");
    fs::create_directories(projRoot / "assets" / "framework" / "data");
    const auto touch = [](const fs::path &p) {
        std::ofstream out(p);
        out << "{}";
    };
    touch(engRoot / "assets" / "framework" / "data" / "eng_only.t3");
    touch(projRoot / "assets" / "framework" / "data" / "proj_only.t3");
    touch(engRoot / "assets" / "framework" / "data" / "shared.t3");
    touch(projRoot / "assets" / "framework" / "data" / "shared.t3");
    fs::create_directories(projRoot / "assets" / "worlds");
    touch(projRoot / "assets" / "worlds" / "main.world");

    auto *db = AssetDataBase::Get();
    db->Reset();
    NativeFileSystemPtr engTsFs  = new NativeFileSystem(engRoot.string());
    NativeFileSystemPtr projTsFs = new NativeFileSystem(projRoot.string());
    db->SetEngineFs(engTsFs);
    db->SetWorkSpaceFs(projTsFs->CreateSubSystem("assets", true));

    const auto &mounts = db->GetMounts();
    ASSERT_GE(mounts.size(), 2u);
    EXPECT_EQ(mounts[0].id, "workspace");
    EXPECT_EQ(mounts[0].displayName, "Project");
    EXPECT_TRUE(mounts[0].writable);
    EXPECT_EQ(mounts[1].id, "engine");
    EXPECT_EQ(mounts[1].displayName, "Engine");
    EXPECT_FALSE(mounts[1].writable);

    auto engAsset = db->RegisterAsset(FilePath("framework/data/eng_only.t3"), false);
    ASSERT_NE(engAsset, nullptr);
    EXPECT_EQ(engAsset->mount, "engine");
    const Uuid engUuid = engAsset->uuid;

    auto projAsset = db->RegisterAsset(FilePath("framework/data/proj_only.t3"), false);
    ASSERT_NE(projAsset, nullptr);
    EXPECT_EQ(projAsset->mount, "workspace");

    // Shadowing: a path present in both mounts resolves to the writable (earlier) mount.
    auto shared = db->RegisterAsset(FilePath("framework/data/shared.t3"), false);
    ASSERT_NE(shared, nullptr);
    EXPECT_EQ(shared->mount, "workspace");

    // Read-only identity is stable across a rescan and no manifest is written for the engine mount.
    EXPECT_TRUE(db->RebuildCacheFromScan().Succeeded());
    auto engAgain = db->RegisterAsset(FilePath("framework/data/eng_only.t3"), false);
    ASSERT_NE(engAgain, nullptr);
    EXPECT_EQ(engAgain->uuid, engUuid);
    EXPECT_FALSE(fs::exists(engRoot / "assets" / "framework" / "data" / "assets.jsonl"));

    // World documents (`.world`) are scanned as source assets (no builder/cook) and get identity.
    auto worldAsset = db->FindAsset(FilePath("worlds/main.world"));
    ASSERT_NE(worldAsset, nullptr);
    EXPECT_EQ(worldAsset->mount, "workspace");

    // Restore the suite fixture's mounts for the remaining tests.
    db->Reset();
    NativeFileSystemPtr fixtureProjectFs = new NativeFileSystem(PROJECT_ROOT);
    db->SetEngineFs(new NativeFileSystem(ENGINE_ROOT));
    db->SetWorkSpaceFs(fixtureProjectFs->CreateSubSystem("assets", true));

    fs::remove_all(engRoot);
    fs::remove_all(projRoot);
}

TEST_F(AssetManagerTest, MigrateLegacyIdentityTest)
{
    namespace fs = std::filesystem;

    const fs::path root = "migrate_tmp";
    fs::remove_all(root);
    fs::create_directories(root / "assets" / "framework" / "data");
    {
        std::ofstream out(root / "assets" / "framework" / "data" / "m.t3");
        out << "{}";
    }

    auto *db = AssetDataBase::Get();
    db->Reset();
    db->SetWorkSpaceFs((new NativeFileSystem(root.string()))->CreateSubSystem("assets", true));

    // Migration seeds the legacy (bundle, path) UUID reproduced via the mount->legacy bundle mapping.
    EXPECT_TRUE(db->MigrateLegacyIdentity().Succeeded());

    auto src = db->FindAsset(FilePath("framework/data/m.t3"));
    ASSERT_NE(src, nullptr);

    uint32_t hash = 0;
    HashCombine32(hash, 2u); // legacy SourceAssetBundle::WORKSPACE ordinal
    HashCombine32(hash, Fnv1a32(src->path.GetStr()));
    EXPECT_TRUE(src->uuid == Uuid::CreateWithSeed(hash));

    // Idempotent: re-running the migration keeps the same identity.
    EXPECT_TRUE(db->MigrateLegacyIdentity().Succeeded());
    EXPECT_TRUE(db->FindAsset(FilePath("framework/data/m.t3"))->uuid == src->uuid);

    // Restore the suite fixture's mounts for the remaining tests.
    db->Reset();
    NativeFileSystemPtr fixtureProjectFs = new NativeFileSystem(PROJECT_ROOT);
    db->SetEngineFs(new NativeFileSystem(ENGINE_ROOT));
    db->SetWorkSpaceFs(fixtureProjectFs->CreateSubSystem("assets", true));

    fs::remove_all(root);
}

TEST_F(AssetManagerTest, BuilderSettingsTest)
{
    auto *manager = AssetBuilderManager::Get();

    // A builder without an override reports no settings (default empty).
    EXPECT_TRUE(manager->GetBuilderSettings(".t1", "common").empty());
    // Unknown extension reports no settings.
    EXPECT_TRUE(manager->GetBuilderSettings(".unknown", "common").empty());

    auto *builder = new SettingsBuilder();
    manager->RegisterBuilder(builder);

    auto settings = manager->GetBuilderSettings(".setup", "any");
    ASSERT_EQ(settings.size(), 2u);
    EXPECT_EQ(settings[0].first, "encode");
    EXPECT_EQ(settings[0].second, "BC7");

    manager->UnRegisterBuilder(builder);
    EXPECT_TRUE(manager->GetBuilderSettings(".setup", "any").empty());
}

TEST_F(AssetManagerTest, MutationTest)
{
    auto *db = AssetDataBase::Get();

    const std::string src   = "framework/data/mut_tmp.t3";
    const std::string moved = "framework/data/mut_moved.t3";
    const std::string dup   = "framework/data/mut_dup.t3";

    auto writeFile = [db](const std::string &path) {
        auto file = db->CreateOrOpenFile(FilePath{FilePath(path)});
        ASSERT_NE(file, nullptr);
        auto archive = file->WriteAsArchive();
        ASSERT_NE(archive, nullptr);
        const char data[] = "{}";
        archive->SaveRaw(data, 2);
        archive->Flush();
    };

    writeFile(src);

    auto asset = db->RegisterAsset(src, false);
    ASSERT_NE(asset, nullptr);
    const Uuid original = asset->uuid;

    // Repeated registration resolves the same manifest identity.
    auto again = db->RegisterAsset(src, false);
    ASSERT_NE(again, nullptr);
    EXPECT_TRUE(again->uuid == original);

    // Move preserves identity.
    auto movedAsset = db->MoveAsset(FilePath{FilePath(src)}, FilePath{FilePath(moved)});
    ASSERT_TRUE(movedAsset);
    EXPECT_TRUE(movedAsset.source->uuid == original);

    // Duplicate assigns a new identity.
    auto dupAsset = db->DuplicateAsset(FilePath{FilePath(moved)}, FilePath{FilePath(dup)});
    ASSERT_EQ(dupAsset.state, AssetCommitState::Committed);
    ASSERT_NE(dupAsset.source, nullptr);
    EXPECT_FALSE(dupAsset.source->uuid == original);

    // Remove drops identity.
    ASSERT_TRUE(db->RemoveAsset(original));
    EXPECT_EQ(db->FindAsset(original), nullptr);

    // Cleanup generated sources and the manifest written by this test.
    auto root = db->GetWorkSpaceFs()->GetPath().GetStr();
    for (const auto &name : {src, moved, dup}) {
        std::filesystem::remove(FilePath(root + "/" + name).GetStr());
    }
    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}

TEST_F(AssetManagerTest, ManifestIdentityTest)
{
    auto *db = AssetDataBase::Get();

    const std::string path   = "framework/data/manifest_seed.t3";
    const Uuid        seeded = Uuid::Create();

    // Pre-write a manifest entry; registration must reuse it regardless of the path.
    auto           fs = db->GetWorkSpaceFs();
    AssetIndexFile manifest("file", "cook");
    IndexFileEntry entry;
    entry.key = "manifest_seed.t3";
    entry.id  = seeded;
    manifest.Set(entry);
    AssetIndexFile::Save(fs, FilePath("framework/data/assets.jsonl"), manifest);
    db->Reset(); // drop cached maps + parsed manifests so the seeded manifest is reloaded

    auto file = db->CreateOrOpenFile(FilePath{FilePath(path)});
    ASSERT_NE(file, nullptr);
    {
        auto archive = file->WriteAsArchive();
        ASSERT_NE(archive, nullptr);
        const char data[] = "{}";
        archive->SaveRaw(data, 2);
        archive->Flush();
    }

    auto asset = db->RegisterAsset(path, false);
    ASSERT_NE(asset, nullptr);
    EXPECT_TRUE(asset->uuid == seeded);

    auto root = fs->GetPath().GetStr();
    std::filesystem::remove(FilePath(root + "/" + path).GetStr());
    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}

TEST_F(AssetManagerTest, OnDemandCookTest)
{
    auto             *db   = AssetDataBase::Get();
    const std::string path = "framework/data/ondemand.t1";

    auto file = db->CreateOrOpenFile(FilePath{FilePath(path)});
    ASSERT_NE(file, nullptr);
    {
        auto archive = file->WriteAsArchive();
        ASSERT_NE(archive, nullptr);
        const char data[] = "{\"val\": 42}";
        archive->SaveRaw(data, sizeof(data) - 1);
        archive->Flush();
    }

    auto src = db->RegisterAsset(path, false);
    ASSERT_NE(src, nullptr);
    const Uuid id = src->uuid;

    // No product exists yet; loading must trigger an in-process on-demand cook.
    auto asset = AssetManager::Get()->LoadAsset<T1Data>(id);
    ASSERT_NE(asset, nullptr);
    asset->BlockUntilLoaded();
    ASSERT_TRUE(asset->IsLoaded());
    EXPECT_EQ(asset->Data().v, 42);

    auto root = db->GetWorkSpaceFs()->GetPath().GetStr();
    std::filesystem::remove(FilePath(root + "/" + path).GetStr());
    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}

TEST_F(AssetManagerTest, LoadFromProductsOnlyTest)
{
    auto             *db   = AssetDataBase::Get();
    const std::string path = "framework/data/products_only.t1";

    auto file = db->CreateOrOpenFile(FilePath{FilePath(path)});
    ASSERT_NE(file, nullptr);
    {
        auto archive = file->WriteAsArchive();
        ASSERT_NE(archive, nullptr);
        const char data[] = "{\"val\": 9}";
        archive->SaveRaw(data, sizeof(data) - 1);
        archive->Flush();
    }
    auto src = db->RegisterAsset(path, false);
    ASSERT_NE(src, nullptr);
    const Uuid id = src->uuid;

    // Produce the product on disk (cook) without ever loading it through the asset loader.
    AssetBuilderManager::Get()->BuildRequestSync(id, "common");
    AssetExecutor::Get()->WaitForAll();

    // Drop the source file and the source catalog: the product alone must load (no source fallback).
    auto root = db->GetWorkSpaceFs()->GetPath().GetStr();
    std::filesystem::remove(FilePath(root + "/" + path).GetStr());
    AssetManager::Get()->SetSourceCatalog(nullptr);

    auto asset = AssetManager::Get()->LoadAssetFromPath<T1Data>("./framework\\data\\nested\\..\\products_only.t1", AssetLoadPolicy::ProductsOnly);
    ASSERT_NE(asset, nullptr);
    asset->BlockUntilLoaded();
    EXPECT_TRUE(asset->IsLoaded());
    EXPECT_EQ(asset->Data().v, 9);

    // A miss with no source catalog fails hard: the loader never falls back to reading source.
    EXPECT_EQ(AssetManager::Get()->LoadAsset(Uuid::Create()), nullptr);

    AssetManager::Get()->SetSourceCatalog(AssetDataBase::Get());
    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}

TEST_F(AssetManagerTest, InProcessCookRunnerTest)
{
    auto             *db   = AssetDataBase::Get();
    const std::string path = "framework/data/ondemand_runner.t1";

    auto file = db->CreateOrOpenFile(FilePath{FilePath(path)});
    ASSERT_NE(file, nullptr);
    {
        auto archive = file->WriteAsArchive();
        ASSERT_NE(archive, nullptr);
        const char data[] = "{\"val\": 7}";
        archive->SaveRaw(data, sizeof(data) - 1);
        archive->Flush();
    }

    auto src = db->RegisterAsset(path, false);
    ASSERT_NE(src, nullptr);
    const Uuid id = src->uuid;

    // Route the on-demand cook through the ICookRunner seam (in-process).
    InProcessCookRunner runner;
    AssetManager::Get()->SetCookRunner(&runner);

    auto asset = AssetManager::Get()->LoadAsset<T1Data>(id);
    ASSERT_NE(asset, nullptr);
    asset->BlockUntilLoaded();
    ASSERT_TRUE(asset->IsLoaded());
    EXPECT_EQ(asset->Data().v, 7);

    AssetManager::Get()->SetCookRunner(nullptr);

    auto root = db->GetWorkSpaceFs()->GetPath().GetStr();
    std::filesystem::remove(FilePath(root + "/" + path).GetStr());
    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}

TEST_F(AssetManagerTest, ScanRebuildTest)
{
    auto *db = AssetDataBase::Get();
    db->Reset();

    // Rebuild the dev cache by scanning the writable mount for builder-known extensions.
    EXPECT_TRUE(db->RebuildCacheFromScan().Succeeded());

    EXPECT_NE(db->FindAsset("framework/data/test_asset.t1"), nullptr);
    EXPECT_NE(db->FindAsset("framework/data/test_asset.t2"), nullptr);
    EXPECT_NE(db->FindAsset("framework/data/test_asset.t3"), nullptr);
}

TEST_F(AssetManagerTest, ImportTest)
{
    auto *db   = AssetDataBase::Get();
    auto  root = db->GetWorkSpaceFs()->GetPath().GetStr();

    const FilePath sourceFile(root + "/framework/data/test_asset.t1");
    const FilePath dest{FilePath("framework/data/imported.t1")};

    // Import assigns identity (manifest entry) and (cook=true) cooks the current platform.
    auto asset = db->ImportAsset(sourceFile, dest, true);
    ASSERT_EQ(asset.state, AssetCommitState::Committed);
    ASSERT_NE(asset.source, nullptr);
    EXPECT_NE(db->FindAsset("framework/data/imported.t1"), nullptr);

    AssetExecutor::Get()->WaitForAll();
    auto loaded = AssetManager::Get()->LoadAsset<T1Data>(asset.source->uuid);
    ASSERT_NE(loaded, nullptr);
    loaded->BlockUntilLoaded();
    EXPECT_TRUE(loaded->IsLoaded());

    std::filesystem::remove(FilePath(root + "/framework/data/imported.t1").GetStr());
    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}

TEST_F(AssetManagerTest, CompressionTest)
{
    ASSERT_TRUE(CompressionManager::Get()->Register(CompressionMethod::LZ4, new DummyCompressor()));
    AssetManager::Get()->SetProductCompression(CompressionMethod::LZ4);

    auto             *db   = AssetDataBase::Get();
    const std::string path = "framework/data/compressed.t1";

    auto file = db->CreateOrOpenFile(FilePath{FilePath(path)});
    ASSERT_NE(file, nullptr);
    {
        auto archive = file->WriteAsArchive();
        ASSERT_NE(archive, nullptr);
        const char data[] = "{\"val\": 7}";
        archive->SaveRaw(data, sizeof(data) - 1);
        archive->Flush();
    }
    file = nullptr;

    auto src = db->RegisterAsset(path, true);
    ASSERT_NE(src, nullptr);
    db->Save(); // flush the build

    auto asset = AssetManager::Get()->LoadAsset<T1Data>(src->uuid);
    ASSERT_NE(asset, nullptr);
    asset->BlockUntilLoaded();
    ASSERT_TRUE(asset->IsLoaded());
    EXPECT_EQ(asset->Data().v, 7);

    AssetManager::Get()->DisableProductCompression();
    CompressionManager::Get()->UnRegister(CompressionMethod::LZ4);

    auto root = db->GetWorkSpaceFs()->GetPath().GetStr();
    std::filesystem::remove(FilePath(root + "/" + path).GetStr());
    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}

TEST_F(AssetManagerTest, MultiTargetTest)
{
    auto *db = AssetDataBase::Get();
    SerializationContext::Get()->Register<MAggData>("MAggData").Member<&MAggData::v>("v");
    AssetBuilderManager::Get()->RegisterBuilder(new MultiTargetBuilder());
    AssetManager::Get()->RegisterAssetHandler<MAggData>();

    const auto bundleRoot = std::filesystem::temp_directory_path() / "sky_multi_target";
    std::filesystem::remove_all(bundleRoot);
    NativeFileSystemPtr taFs = new NativeFileSystem(FilePath((bundleRoot / "ta").string()));
    NativeFileSystemPtr tbFs = new NativeFileSystem(FilePath((bundleRoot / "tb").string()));
    AssetManager::Get()->AddAssetProductBundle(new HashedAssetBundle(taFs, "ta"));
    AssetManager::Get()->AddAssetProductBundle(new HashedAssetBundle(tbFs, "tb"));

    CookConfig config;
    ASSERT_TRUE(config.Parse(R"({"targets":{"ta":{"bundle":"ta"},"tb":{"bundle":"tb"}}})"));
    AssetBuilderManager::Get()->SetCookConfig(config);

    const std::string path = "framework/data/multi.mt";
    auto              file = db->CreateOrOpenFile(FilePath{FilePath(path)});
    ASSERT_NE(file, nullptr);
    {
        auto archive = file->WriteAsArchive();
        ASSERT_NE(archive, nullptr);
        const char data[] = "{\"val\": 5}";
        archive->SaveRaw(data, sizeof(data) - 1);
        archive->Flush();
    }
    file = nullptr;

    auto src = db->RegisterAsset(path, false);
    ASSERT_NE(src, nullptr);

    // One source -> one product per configured target (same uuid, per-bundle).
    db->BuildAllTargets(src->uuid);
    AssetExecutor::Get()->WaitForAll();

    const auto taIndex = AssetIndexFile::Load(taFs, FilePath("product.index"), "path");
    const auto tbIndex = AssetIndexFile::Load(tbFs, FilePath("product.index"), "path");
    EXPECT_NE(taIndex.Find("framework/data/multi.mt"), nullptr);
    EXPECT_NE(tbIndex.Find("framework/data/multi.mt"), nullptr);

    auto asset = AssetManager::Get()->LoadAsset<MAggData>(src->uuid);
    ASSERT_NE(asset, nullptr);
    asset->BlockUntilLoaded();
    EXPECT_TRUE(asset->IsLoaded());
    EXPECT_EQ(asset->Data().v, 5);

    AssetBuilderManager::Get()->SetCookConfig(CookConfig{});
    std::filesystem::remove_all(bundleRoot);

    auto root = db->GetWorkSpaceFs()->GetPath().GetStr();
    std::filesystem::remove(FilePath(root + "/" + path).GetStr());
    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}

TEST_F(AssetManagerTest, BuildRequestAlwaysCompletes)
{
    auto *manager = AssetBuilderManager::Get();
    auto *db      = AssetDataBase::Get();
    auto  root    = db->GetWorkSpaceFs()->GetPath().GetStr();

    // Unknown asset: the completion still fires FAILED so an on-demand load never hangs waiting.
    bool             called = false;
    AssetBuildResult first{};
    manager->BuildRequest(Uuid::Create(), "common", [&](const AssetBuildResult &r) {
        called = true;
        first  = r;
    });
    AssetExecutor::Get()->WaitForAll();
    EXPECT_TRUE(called);
    EXPECT_EQ(first.retCode, AssetBuildRetCode::FAILED);
    EXPECT_FALSE(first.error.empty());

    // Known source with no registered builder: also completes FAILED (no null deref).
    const std::string path = "framework/data/nobuilder.zzz";
    {
        std::ofstream out(FilePath(root + "/" + path).GetStr());
        out << "x";
    }
    auto src = db->RegisterAsset(path, false);
    ASSERT_NE(src, nullptr);

    bool called2 = false;
    manager->BuildRequest(src->uuid, "common", [&](const AssetBuildResult &r) { called2 = (r.retCode == AssetBuildRetCode::FAILED); });
    AssetExecutor::Get()->WaitForAll();
    EXPECT_TRUE(called2);

    std::filesystem::remove(FilePath(root + "/" + path).GetStr());
    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}

TEST_F(AssetManagerTest, TypeIdentityTest)
{
    // The builder registry reports the same AssetTypeId as the compile-time trait.
    auto *builder = AssetBuilderManager::Get()->QueryBuilder(".t1");
    ASSERT_NE(builder, nullptr);
    EXPECT_EQ(std::string(builder->QueryType(".t1")), std::string(AssetTraits<T1Data>::ASSET_TYPE));

    // The source-derived type matches.
    auto             *db   = AssetDataBase::Get();
    auto              root = db->GetWorkSpaceFs()->GetPath().GetStr();
    const std::string path = "framework/data/type_id.t1";

    auto file = db->CreateOrOpenFile(FilePath{FilePath(path)});
    ASSERT_NE(file, nullptr);
    {
        auto       archive = file->WriteAsArchive();
        const char data[]  = "{\"val\": 1}";
        archive->SaveRaw(data, sizeof(data) - 1);
        archive->Flush();
    }
    file = nullptr;

    auto src = db->RegisterAsset(path, false);
    ASSERT_NE(src, nullptr);

    std::string type;
    ASSERT_TRUE(db->GetType(src->uuid, type));
    EXPECT_EQ(type, std::string(AssetTraits<T1Data>::ASSET_TYPE));

    std::filesystem::remove(FilePath(root + "/" + path).GetStr());
    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}

TEST_F(AssetManagerTest, ConcurrentLoadTest)
{
    auto             *db   = AssetDataBase::Get();
    auto              root = db->GetWorkSpaceFs()->GetPath().GetStr();
    const std::string path = "framework/data/concurrent.t1";

    auto file = db->CreateOrOpenFile(FilePath{FilePath(path)});
    ASSERT_NE(file, nullptr);
    {
        auto       archive = file->WriteAsArchive();
        const char data[]  = "{\"val\": 9}";
        archive->SaveRaw(data, sizeof(data) - 1);
        archive->Flush();
    }
    file = nullptr;

    auto src = db->RegisterAsset(path, false);
    ASSERT_NE(src, nullptr);

    // Concurrent loads of an unbuilt asset must coalesce onto one in-process cook without deadlock.
    std::vector<std::shared_ptr<Asset<T1Data>>> results(4);
    std::vector<std::thread>                    threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([src, i, &results]() { results[i] = AssetManager::Get()->LoadAsset<T1Data>(src->uuid); });
    }
    for (auto &thread : threads) {
        thread.join();
    }

    for (auto &result : results) {
        ASSERT_NE(result, nullptr);
        result->BlockUntilLoaded();
        EXPECT_TRUE(result->IsLoaded());
        EXPECT_EQ(result->Data().v, 9);
    }

    std::filesystem::remove(FilePath(root + "/" + path).GetStr());
    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}

TEST_F(AssetManagerTest, ConcurrentLoadStress)
{
    auto *db   = AssetDataBase::Get();
    auto  root = db->GetWorkSpaceFs()->GetPath().GetStr();

    constexpr int kIterations = 25;
    constexpr int kThreads    = 8;

    for (int it = 0; it < kIterations; ++it) {
        const std::string path = "framework/data/concurrent_stress_" + std::to_string(it) + ".t1";

        auto file = db->CreateOrOpenFile(FilePath{FilePath(path)});
        ASSERT_NE(file, nullptr);
        {
            auto       archive = file->WriteAsArchive();
            const char data[]  = "{\"val\": 7}";
            archive->SaveRaw(data, sizeof(data) - 1);
            archive->Flush();
        }
        file = nullptr;

        auto src = db->RegisterAsset(path, false);
        ASSERT_NE(src, nullptr);

        // Hammer one UUID from many threads; every waiter must unblock (no orphaned wait handle, no
        // overwritten future) and the asset must end LOADED with the cooked value.
        std::vector<std::shared_ptr<Asset<T1Data>>> results(kThreads);
        std::vector<std::thread>                    threads;
        threads.reserve(kThreads);
        for (int i = 0; i < kThreads; ++i) {
            threads.emplace_back([src, i, &results]() { results[i] = AssetManager::Get()->LoadAsset<T1Data>(src->uuid); });
        }
        for (auto &thread : threads) {
            thread.join();
        }

        for (auto &result : results) {
            ASSERT_NE(result, nullptr);
            result->BlockUntilLoaded();
            ASSERT_TRUE(result->IsLoaded());
            EXPECT_EQ(result->Data().v, 7);
        }

        std::filesystem::remove(FilePath(root + "/" + path).GetStr());
    }

    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}

TEST_F(AssetManagerTest, CookWorkerTest)
{
    auto             *db   = AssetDataBase::Get();
    auto              root = db->GetWorkSpaceFs()->GetPath().GetStr();
    const std::string path = "framework/data/batch.t1";

    auto file = db->CreateOrOpenFile(FilePath{FilePath(path)});
    ASSERT_NE(file, nullptr);
    {
        auto       archive = file->WriteAsArchive();
        const char data[]  = "{\"val\": 3}";
        archive->SaveRaw(data, sizeof(data) - 1);
        archive->Flush();
    }
    file = nullptr;

    auto src = db->RegisterAsset(path, false);
    ASSERT_NE(src, nullptr);

    // Batch cook the work list, then load the freshly produced product.
    CookWorker worker;
    worker.CookBatch({CookWorker::Job{src->uuid, "common"}});

    auto asset = AssetManager::Get()->LoadAsset<T1Data>(src->uuid);
    ASSERT_NE(asset, nullptr);
    asset->BlockUntilLoaded();
    EXPECT_TRUE(asset->IsLoaded());
    EXPECT_EQ(asset->Data().v, 3);

    std::filesystem::remove(FilePath(root + "/" + path).GetStr());
    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}
TEST_F(AssetManagerTest, BulkRoundTripTest)
{
    auto *db = AssetDataBase::Get();

    const auto bundleRoot = std::filesystem::temp_directory_path() / "sky_bulk_test";
    std::filesystem::remove_all(bundleRoot);
    NativeFileSystemPtr bundleFs = new NativeFileSystem(FilePath((bundleRoot / "b").string()));
    AssetManager::Get()->AddAssetProductBundle(new HashedAssetBundle(bundleFs, "bulk"));

    const std::string path = "framework/data/bulk.bulk";
    auto              file = db->CreateOrOpenFile(FilePath{FilePath(path)});
    ASSERT_NE(file, nullptr);
    {
        auto       archive = file->WriteAsArchive();
        const char data[]  = "{}";
        archive->SaveRaw(data, 2);
        archive->Flush();
    }
    file     = nullptr;
    auto src = db->RegisterAsset(path, false);
    ASSERT_NE(src, nullptr);
    const Uuid id = src->uuid;

    const std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04};

    {
        auto asset =
            std::static_pointer_cast<Asset<BulkData>>(AssetManager::Get()->CreateBuildAsset(id, Name(AssetTraits<BulkData>::ASSET_TYPE.data())));
        ASSERT_NE(asset, nullptr);
        asset->Data().magic  = 0xC0DE;
        asset->Data().offset = 0;
        asset->Data().size   = payload.size();
        asset->AddBulk(payload.data(), payload.size());
        ASSERT_TRUE(AssetManager::Get()->SaveAsset(asset, "bulk"));
    }

    // Reload from the product only: the header is resident, the payload is read on demand.
    auto loaded = AssetManager::Get()->LoadAsset<BulkData>(id);
    ASSERT_NE(loaded, nullptr);
    loaded->BlockUntilLoaded();
    ASSERT_TRUE(loaded->IsLoaded());
    EXPECT_EQ(loaded->Data().magic, 0xC0DEu);
    EXPECT_EQ(loaded->Data().size, payload.size());
    EXPECT_TRUE(loaded->HasBulk());
    EXPECT_EQ(loaded->ReadBulk(loaded->Data().offset, loaded->Data().size), payload);
    EXPECT_TRUE(loaded->ReadBulk(4, 0).empty());

    loaded.reset(); // release the product stream before removing the temp bundle

    std::filesystem::remove_all(bundleRoot);
    auto root = db->GetWorkSpaceFs()->GetPath().GetStr();
    std::filesystem::remove(FilePath(root + "/" + path).GetStr());
    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}

TEST_F(AssetManagerTest, NoBulkRoundTripTest)
{
    auto *db = AssetDataBase::Get();

    const auto bundleRoot = std::filesystem::temp_directory_path() / "sky_nobulk_test";
    std::filesystem::remove_all(bundleRoot);
    NativeFileSystemPtr bundleFs = new NativeFileSystem(FilePath((bundleRoot / "b").string()));
    AssetManager::Get()->AddAssetProductBundle(new HashedAssetBundle(bundleFs, "nobulk"));

    const std::string path = "framework/data/nobulk.bulk";
    auto              file = db->CreateOrOpenFile(FilePath{FilePath(path)});
    ASSERT_NE(file, nullptr);
    {
        auto       archive = file->WriteAsArchive();
        const char data[]  = "{}";
        archive->SaveRaw(data, 2);
        archive->Flush();
    }
    file     = nullptr;
    auto src = db->RegisterAsset(path, false);
    ASSERT_NE(src, nullptr);
    const Uuid id = src->uuid;

    {
        auto asset =
            std::static_pointer_cast<Asset<BulkData>>(AssetManager::Get()->CreateBuildAsset(id, Name(AssetTraits<BulkData>::ASSET_TYPE.data())));
        ASSERT_NE(asset, nullptr);
        asset->Data().magic = 0x1234;
        ASSERT_TRUE(AssetManager::Get()->SaveAsset(asset, "nobulk"));
    }

    auto loaded = AssetManager::Get()->LoadAsset<BulkData>(id);
    ASSERT_NE(loaded, nullptr);
    loaded->BlockUntilLoaded();
    ASSERT_TRUE(loaded->IsLoaded());
    EXPECT_EQ(loaded->Data().magic, 0x1234u);
    EXPECT_FALSE(loaded->HasBulk());
    EXPECT_TRUE(loaded->ReadBulk(0, 4).empty());

    std::filesystem::remove_all(bundleRoot);
    auto root = db->GetWorkSpaceFs()->GetPath().GetStr();
    std::filesystem::remove(FilePath(root + "/" + path).GetStr());
    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}

TEST_F(AssetScanTest, EngineIdentityCannotBeRemovedOrMovedThroughDatabase)
{
    std::ofstream(engine / "protected.world") << "{}";
    const auto source = database.RegisterAsset("protected.world", false);
    ASSERT_TRUE(source);
    EXPECT_FALSE(database.RemoveAsset(source->uuid));
    EXPECT_FALSE(database.MoveAsset(source->path, "renamed.world"));
    EXPECT_EQ(database.FindAsset(source->uuid).Get(), source.Get());
    EXPECT_EQ(source->path, FilePath("protected.world"));
    EXPECT_TRUE(std::filesystem::exists(engine / "protected.world"));
    EXPECT_FALSE(std::filesystem::exists(engine / "renamed.world"));
    EXPECT_FALSE(std::filesystem::exists(workspace / "renamed.world"));
}

TEST_F(AssetScanTest, SameRelativePathKeepsBothMountIdentitiesAndOwnedReads)
{
    const auto engineId = Uuid::CreateFromString("8F015801-3437-4A44-A581-C191184C8101");
    std::ofstream(engine / "scene.world") << "engine";
    std::ofstream(engine / "assets.jsonl") << "{\"file\":\"scene.world\",\"id\":\"" << engineId.ToString()
                                           << "\",\"cook\":{\"targets\":[\"engine\"]}}\n";
    ASSERT_TRUE(database.RebuildCacheFromScan().Succeeded());
    const auto projectSource = database.FindAsset("workspace", FilePath("scene.world"));
    const auto engineSource  = database.FindAsset("engine", FilePath("scene.world"));
    ASSERT_NE(projectSource, nullptr);
    ASSERT_NE(engineSource, nullptr);
    EXPECT_NE(projectSource->uuid, engineSource->uuid);
    EXPECT_EQ(engineSource->uuid, engineId);
    EXPECT_EQ(database.FindAsset(FilePath("scene.world")).Get(), projectSource.Get());
    EXPECT_EQ(database.RegisterAsset("engine", FilePath("scene.world"), false).Get(), engineSource.Get());
    std::string contents, absolute;
    ASSERT_TRUE(database.OpenFile(engineSource)->ReadString(contents));
    EXPECT_EQ(contents, "engine");
    ASSERT_TRUE(database.GetAbsoluteSourcePath(engineId, absolute));
    EXPECT_EQ(FilePath(absolute), FilePath(engine / "scene.world"));
    EXPECT_FALSE(database.GetCookJson(engineId).empty());
    EXPECT_FALSE(database.SetCookJson(engineId, "{}"));
    EXPECT_FALSE(database.PersistSourceIdentity(engineId));
    EXPECT_FALSE(database.DeleteAsset(engineId));
    const auto projectId = projectSource->uuid;
    database.Save();
    database.Reset();
    ASSERT_TRUE(database.Load().Succeeded());
    EXPECT_EQ(database.FindAsset("engine", FilePath("scene.world"))->uuid, engineId);
    EXPECT_EQ(database.FindAsset("workspace", FilePath("scene.world"))->uuid, projectId);
    EXPECT_EQ(database.GetSources().size(), 2U);
}

TEST_F(AssetScanTest, DeletedSourceDoesNotReturnFromStaleDevelopmentCache)
{
    const auto source = database.RegisterAsset("scene.world", false);
    ASSERT_NE(source, nullptr);
    const auto id = source->uuid;
    database.Save();
    ASSERT_TRUE(database.DeleteAsset(id));
    EXPECT_FALSE(database.GetWorkSpaceFs()->FileExist("scene.world"));
    database.Reset();
    ASSERT_TRUE(database.Load().Succeeded());
    EXPECT_FALSE(database.FindAsset(id));
    EXPECT_FALSE(database.FindAsset(FilePath("scene.world")));
}

TEST_F(AssetScanTest, LegacyCacheWithoutMountUsesCommittedOwnerBeforeOverlayOrder)
{
    const auto engineId = Uuid::Create();
    std::ofstream(engine / "scene.world") << "engine";
    std::ofstream(engine / "assets.jsonl") << "{\"file\":\"scene.world\",\"id\":\"" << engineId.ToString() << "\"}\n";
    std::ofstream(workspace / "assets.db") << "{\"assets\":[{\"uuid\":\"" << engineId.ToString()
                                           << "\",\"markedName\":\"legacy\",\"ext\":\".world\",\"path\":\"scene.world\",\"dependencies\":[]}]}";
    ASSERT_TRUE(database.Load().Succeeded());
    const auto engineSource  = database.FindAsset("engine", FilePath("scene.world"));
    const auto projectSource = database.FindAsset("workspace", FilePath("scene.world"));
    ASSERT_NE(engineSource, nullptr);
    ASSERT_NE(projectSource, nullptr);
    EXPECT_EQ(engineSource->uuid, engineId);
    EXPECT_EQ(engineSource->name, "legacy");
    EXPECT_NE(projectSource->uuid, engineId);
    EXPECT_EQ(database.FindAsset(FilePath("scene.world")).Get(), projectSource.Get());
}

TEST_F(AssetScanTest, DamagedDirectoryRetainsOnlyValidIdentitiesAndNeverSeeds)
{
    const auto id = Uuid::Create();
    std::ofstream(workspace / "missing.world") << "{}";
    const std::string bytes = "{\"file\":\"scene.world\",\"id\":\"" + id.ToString() + "\"}\ninvalid\n";
    std::ofstream(workspace / "assets.jsonl") << bytes;
    const auto report = database.Load();
    EXPECT_TRUE(report.Succeeded());
    ASSERT_EQ(report.blockedDirs.size(), 1U);
    ASSERT_EQ(report.warnings.size(), 1U);
    EXPECT_EQ(report.warnings.front().line, 2U);
    const auto scene = database.FindAsset("scene.world");
    ASSERT_NE(scene, nullptr);
    EXPECT_EQ(scene->uuid, id);
    EXPECT_EQ(database.FindAsset("missing.world"), nullptr);
    const auto migration = database.MigrateLegacyIdentity();
    EXPECT_FALSE(migration.Complete());
    std::ifstream input(workspace / "assets.jsonl");
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>(input), {}), bytes);
}

TEST_F(AssetScanTest, DuplicateIdentityAcrossDirectoriesIsNotPublishedOrRewritten)
{
    const auto id = Uuid::Create();
    std::filesystem::create_directory(workspace / "other");
    std::ofstream(workspace / "other" / "scene.world") << "{}";
    AssetIndexFile manifest("file");
    manifest.Set({"scene.world", id, {}});
    ASSERT_TRUE(AssetIndexFile::Save(database.GetWorkSpaceFs(), "assets.jsonl", manifest));
    ASSERT_TRUE(AssetIndexFile::Save(database.GetWorkSpaceFs(), "other/assets.jsonl", manifest));
    const auto report = database.RebuildCacheFromScan();
    EXPECT_TRUE(report.Succeeded());
    EXPECT_EQ(report.blockedDirs.size(), 2U);
    EXPECT_EQ(database.FindAsset(id), nullptr);
}

TEST_F(AssetScanTest, ImportRejectsOccupiedAndEscapingPathsWithoutAppending)
{
    const FilePath source(root / "external.world");
    std::ofstream(root / "external.world") << "new bytes";
    const auto occupied = database.ImportAsset(source, "scene.world", false);
    EXPECT_EQ(occupied.state, AssetCommitState::NotCommitted);
    EXPECT_EQ(occupied.status.error, FileOperationError::AlreadyExists);
    std::ifstream input(workspace / "scene.world");
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>(input), {}), "{}");
    for (const auto *path : {"../escaped.world", "./noncanonical.world", "a/../../escaped.world"}) {
        const auto result = database.ImportAsset(source, path, false);
        EXPECT_EQ(result.state, AssetCommitState::NotCommitted);
        EXPECT_EQ(result.status.error, FileOperationError::InvalidPath);
    }
    const auto created = database.ImportAsset(source, "imported.world", false);
    ASSERT_EQ(created.state, AssetCommitState::Committed);
    ASSERT_NE(created.source, nullptr);
    EXPECT_EQ(created.source->mount, "workspace");
}

TEST_F(AssetScanTest, ScanReadsAndWritesPerDirectoryRatherThanPerAsset)
{
    class CountingFileSystem final : public NativeFileSystem {
    public:
        explicit CountingFileSystem(const FilePath &path) : NativeFileSystem(path)
        {
        }
        FilePtr OpenFile(const FilePath &path) override
        {
            if (path.FileName() == "assets.jsonl")
                ++reads;
            return NativeFileSystem::OpenFile(path);
        }
        FileOperationStatus AcquireFileLock(const FilePath &path, FileLockPtr &out) const override
        {
            ++locks;
            return NativeFileSystem::AcquireFileLock(path, out);
        }
        FileOperationStatus ReplaceFileAtomic(const FilePath &from, const FilePath &to) const override
        {
            if (to.FileName() == "assets.jsonl")
                ++saves;
            return NativeFileSystem::ReplaceFileAtomic(from, to);
        }
        mutable size_t reads = 0, locks = 0, saves = 0;
    };
    for (const size_t directories : {size_t{1}, size_t{4}}) {
        for (const size_t files : {size_t{10}, size_t{100}, size_t{1000}}) {
            database.Reset();
            const auto                     batch      = workspace / (std::to_string(directories) + "-" + std::to_string(files));
            CounterPtr<CountingFileSystem> filesystem = new CountingFileSystem(FilePath(batch));
            database.SetWorkSpaceFs(filesystem);
            for (size_t directory = 0; directory < directories; ++directory) {
                const auto      path = batch / std::to_string(directory);
                std::error_code error;
                std::filesystem::create_directories(path, error);
                ASSERT_FALSE(error);
                for (size_t file = 0; file < files; ++file)
                    std::ofstream(path / (std::to_string(file) + ".world")) << "{}";
            }
            const auto begin = std::chrono::steady_clock::now();
            EXPECT_TRUE(database.RebuildCacheFromScan().Complete());
            const auto cold = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin).count();
            EXPECT_LE(filesystem->reads, directories * 2);
            EXPECT_EQ(filesystem->locks, directories);
            EXPECT_EQ(filesystem->saves, directories);
            filesystem->reads = filesystem->locks = filesystem->saves = 0;
            const auto warmBegin                                      = std::chrono::steady_clock::now();
            EXPECT_TRUE(database.RebuildCacheFromScan().Complete());
            const auto warm = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - warmBegin).count();
            EXPECT_EQ(filesystem->reads, directories);
            EXPECT_EQ(filesystem->locks, 0U);
            EXPECT_EQ(filesystem->saves, 0U);
            std::cout << "SCAN_SCALE dirs=" << directories << " files_per_dir=" << files << " cold_us=" << cold << " warm_us=" << warm << '\n';
        }
    }
}

TEST_F(AssetScanTest, FailedCreationCommitReportsCleanupAndLeavesNoRegistration)
{
    class FailingFileSystem final : public NativeFileSystem {
    public:
        explicit FailingFileSystem(const FilePath &path) : NativeFileSystem(path)
        {
        }
        FileOperationStatus ReplaceFileAtomic(const FilePath &, const FilePath &) const override
        {
            return {FileOperationError::IOError, "injected manifest commit failure"};
        }
        FileOperationStatus RemoveFile(const FilePath &path) const override
        {
            return failCleanup ? FileOperationStatus{FileOperationError::IOError, "injected cleanup failure"} : NativeFileSystem::RemoveFile(path);
        }
        bool failCleanup = false;
    };
    CounterPtr<FailingFileSystem> filesystem = new FailingFileSystem(FilePath(workspace));
    database.SetWorkSpaceFs(filesystem);
    std::ofstream(root / "source.world") << "source";
    const auto first = database.ImportAsset(FilePath(root / "source.world"), "copy.world", false);
    EXPECT_EQ(first.state, AssetCommitState::NotCommitted);
    EXPECT_FALSE(first.status);
    EXPECT_TRUE(first.recovery.empty());
    EXPECT_FALSE(filesystem->FileExist("copy.world"));
    EXPECT_EQ(database.FindAsset("copy.world"), nullptr);
    filesystem->failCleanup = true;
    const auto second       = database.ImportAsset(FilePath(root / "source.world"), "copy.world", false);
    EXPECT_EQ(second.state, AssetCommitState::RollbackFailed);
    ASSERT_EQ(second.recovery.size(), 2U);
    EXPECT_EQ(second.recovery.back().path, FilePath(workspace / "copy.world"));
    EXPECT_EQ(database.FindAsset("copy.world"), nullptr);
}

TEST_F(AssetScanTest, EngineOnlyLoadDoesNotRequireWorkspaceOrWriteManifest)
{
    database.SetWorkSpaceFs({});
    std::ofstream(engine / "engine.world") << "{}";
    EXPECT_TRUE(database.Load().Complete());
    EXPECT_NE(database.FindAsset("engine", "engine.world"), nullptr);
    EXPECT_FALSE(std::filesystem::exists(engine / "assets.jsonl"));
    EXPECT_FALSE(std::filesystem::exists(engine / "assets.jsonl.lock"));
}

TEST_F(AssetScanTest, LockedRereadPreservesWorkerGeneratedCommitAfterPlanningRead)
{
    class InterleavedFileSystem final : public NativeFileSystem {
    public:
        explicit InterleavedFileSystem(const FilePath &path) : NativeFileSystem(path)
        {
        }
        FileOperationStatus AcquireFileLock(const FilePath &path, FileLockPtr &out) const override
        {
            if (beforeLock) {
                const auto callback = std::move(beforeLock);
                callback();
            }
            return NativeFileSystem::AcquireFileLock(path, out);
        }
        mutable std::function<void()> beforeLock;
    };
    const auto owner = database.RegisterAsset("scene.world", false);
    ASSERT_NE(owner, nullptr);
    AssetDataBase worker;
    worker.SetWorkSpaceFs(new NativeFileSystem(FilePath(workspace)));
    ASSERT_NE(worker.RegisterAsset("scene.world", false), nullptr);
    CounterPtr<InterleavedFileSystem> filesystem = new InterleavedFileSystem(FilePath(workspace));
    database.SetWorkSpaceFs(filesystem);
    std::ofstream(workspace / "new.world") << "{}";
    GeneratedAssetManifest children;
    filesystem->beforeLock = [&] {
        const std::array<GeneratedAssetDescriptor, 1> requested{{{"child", "World"}}};
        ASSERT_TRUE(worker.UpdateGeneratedProducts(owner->uuid, requested, {}, children));
    };
    const auto report = database.RebuildCacheFromScan();
    EXPECT_TRUE(report.Complete());
    ASSERT_EQ(children.records.size(), 1U);
    const auto persisted = AssetIndexFile::Load(filesystem, "assets.jsonl", "file", "cook");
    ASSERT_NE(persisted.Find("scene.world"), nullptr);
    EXPECT_FALSE(persisted.Find("scene.world")->generated.empty());
    EXPECT_TRUE(database.Exists(children.records.front().uuid));
    EXPECT_NE(database.FindAsset("new.world"), nullptr);
}

TEST_F(AssetScanTest, StaleCacheCannotHideManifestIdentityInAnotherDirectory)
{
    std::filesystem::create_directory(workspace / "z");
    std::filesystem::create_directory(workspace / "a");
    std::ofstream(workspace / "z" / "old.world") << "{}";
    const auto old = database.RegisterAsset("z/old.world", false);
    ASSERT_NE(old, nullptr);
    const auto id = old->uuid;
    database.Save();
    database.Reset();
    std::filesystem::remove(workspace / "z" / "assets.jsonl");
    std::ofstream(workspace / "a" / "current.world") << "{}";
    AssetIndexFile manifest("file");
    manifest.Set({"current.world", id, {}});
    ASSERT_TRUE(AssetIndexFile::Save(database.GetWorkSpaceFs(), "a/assets.jsonl", manifest));
    const auto report = database.Load();
    EXPECT_TRUE(report.Succeeded());
    EXPECT_FALSE(report.blockedDirs.empty());
    const auto current = database.FindAsset(id);
    ASSERT_NE(current, nullptr);
    EXPECT_EQ(current->path, FilePath("a/current.world"));
    EXPECT_EQ(database.FindAsset("z/old.world"), nullptr);
    EXPECT_FALSE(std::filesystem::exists(workspace / "z" / "assets.jsonl"));
}
