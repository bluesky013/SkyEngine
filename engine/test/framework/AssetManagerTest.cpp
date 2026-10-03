//
// Created by blues on 2024/6/21.
//


#include <gtest/gtest.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetBuilder.h>

#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/CoreReflection.h>
#include <framework/platform/PlatformBase.h>
#include <test/EngineRoot.h>
#include <framework/asset/AssetIndexFile.h>
#include <framework/asset/CookConfig.h>
#include <framework/asset/AssetProductBundle.h>
#include <framework/asset/CookWorker.h>
#include <framework/asset/InProcessCookRunner.h>
#include <framework/compression/Compressor.h>

#include <cstring>
#include <filesystem>
#include <span>
#include <thread>

using namespace sky;

// Identity codec used to exercise the compressed product path without the CompressionModule.
class DummyCompressor : public ICompressor {
public:
    uint32_t CompressBound(uint32_t inDataSize) override { return inDataSize; }
    CompressResult Compress(const std::span<const uint8_t> &inData, const std::span<uint8_t> &out, uint32_t) override
    {
        if (out.size() < inData.size()) {
            return { false, 0 };
        }
        std::memcpy(out.data(), inData.data(), inData.size());
        return { true, static_cast<uint32_t>(inData.size()) };
    }
    CompressResult DeCompress(const std::span<const uint8_t> &inData, const std::span<uint8_t> &out, uint32_t) override
    {
        if (out.size() < inData.size()) {
            return { false, 0 };
        }
        std::memcpy(out.data(), inData.data(), inData.size());
        return { true, static_cast<uint32_t>(inData.size()) };
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
        auto archive = request.file->ReadAsArchive();
        JsonInputArchive json(*archive);
        json.Start("val");
        const int v = json.LoadInt();
        json.End();

        auto *am = AssetManager::Get();
        auto asset = std::static_pointer_cast<Asset<MAggData>>(am->FindOrCreateAsset(request.assetInfo->uuid, Name("MAgg")));
        asset->Data().v = v;
        am->SaveAsset(asset, request.target);
        result.retCode = AssetBuildRetCode::SUCCESS;
    }

    const std::vector<std::string> &GetExtensions() const override
    {
        static std::vector<std::string> ext = { ".mt" };
        return ext;
    }

    std::string_view QueryType(const std::string &) const override { return AssetTraits<MAggData>::ASSET_TYPE; }
};

struct T1Data : public RefObject {
    int v;
};

struct T2Data : public RefObject {
    float v;
    int extVal;
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

class TestBuilder1 : public AssetBuilder {
public:
    TestBuilder1() = default;
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
        auto archive = cfg->OpenFile("asset_cfg_t2.json")->ReadAsArchive();
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
        auto archive = request.file->ReadAsArchive();
        JsonInputArchive json(*archive);

        auto *am = AssetManager::Get();
        auto *builder = AssetBuilderManager::Get()->QueryBuilder(request.assetInfo->ext);
        if (builder != nullptr && builder->QueryType(request.assetInfo->ext) == AssetTraits<T1Data>::ASSET_TYPE) {
            auto asset = std::static_pointer_cast<Asset<T1Data>>(am->FindOrCreateAsset(request.assetInfo->uuid, Name("T1")));

            T1Data &data = asset->Data();
            json.Start("val");
            data.v = json.LoadInt();

            am->SaveAsset(asset, "common");
        } else {
            auto asset = std::static_pointer_cast<Asset<T2Data>>(am->FindOrCreateAsset(request.assetInfo->uuid, Name("T2")));

            T2Data &data = asset->Data();
            json.Start("val");
            data.v = static_cast<float>(json.LoadDouble());

            auto target = request.target.empty() ? GetDefaultBundle() : request.target;

            auto iter = configs.find(target);
            if (iter != configs.end()) {
                data.extVal = iter->second.extVal;
                am->SaveAsset(asset, target);
            }
        }
    }

    std::string_view QueryType(const std::string &ext) const override
    {
        return ext == ".t1" ? AssetTraits<T1Data>::ASSET_TYPE : AssetTraits<T2Data>::ASSET_TYPE;
    }

private:
    std::unordered_map<std::string, Config> configs;
    std::unordered_map<PlatformType, std::string> defaultBundles;
};

class TestBuilder2 : public AssetBuilder {
public:
    TestBuilder2() = default;
    ~TestBuilder2() override = default;

    void Request(const AssetBuildRequest &request, AssetBuildResult &result) override
    {
        auto archive = request.file->ReadAsArchive();
        JsonInputArchive json(*archive);

        json.Start("v1");
        std::string p1 = json.LoadString();
        json.End();

        json.Start("v2");
        std::string p2 = json.LoadString();
        json.End();

        auto *am = AssetManager::Get();
        auto asset = std::static_pointer_cast<Asset<T3Data>>(am->FindOrCreateAsset(request.assetInfo->uuid, Name("T3")));

        auto p1Asset = AssetDataBase::Get()->RegisterAsset(p1);
        auto p2Asset = AssetDataBase::Get()->RegisterAsset(p2);
        ASSERT_NE(p1Asset, nullptr);
        ASSERT_NE(p2Asset, nullptr);

        request.assetInfo->dependencies.emplace_back(p1Asset->uuid);
        request.assetInfo->dependencies.emplace_back(p2Asset->uuid);

        auto &data = asset->Data();
        data.t1 = p1Asset->uuid;
        data.t2 = p2Asset->uuid;

        asset->AddDependencies(p1Asset->uuid);
        asset->AddDependencies(p2Asset->uuid);

        am->SaveAsset(asset, "common");
        result.retCode = AssetBuildRetCode::SUCCESS;
    }

    const std::vector<std::string> &GetExtensions() const override
    {
        static std::vector<std::string> ext = {".t3"};
        return ext;
    }

    std::string_view QueryType(const std::string &ext) const override { return AssetTraits<T3Data>::ASSET_TYPE; }
};

class AssetManagerTest : public ::testing::Test {
public:
    static void SetUpTestSuite()
    {
        auto *context = SerializationContext::Get();

        context->Register<T1Data>("T1Data")
                .Member<&T1Data::v>("v");

        context->Register<T2Data>("T2Data")
                .Member<&T2Data::v>("v")
                .Member<&T2Data::extVal>("extVal");

        context->Register<T3Data>("T3Data")
                .Member<&T3Data::t1>("t1")
                .Member<&T3Data::t2>("t2");

        NativeFileSystemPtr projectFs = new NativeFileSystem(PROJECT_ROOT);
        AssetDataBase::Get()->SetEngineFs(new NativeFileSystem(ENGINE_ROOT));
        AssetDataBase::Get()->SetWorkSpaceFs(projectFs->CreateSubSystem("assets", true));
        AssetManager::Get()->SetSourceCatalog(AssetDataBase::Get());

        AssetBuilderManager::Get()->RegisterBuilder(new TestBuilder1());
        AssetBuilderManager::Get()->RegisterBuilder(new TestBuilder2());

        AssetBuilderManager::Get()->SetWorkSpaceFs(projectFs);
        AssetBuilderManager::Get()->LoadBuildConfigs(projectFs->CreateSubSystem("configs", false));

        AssetManager::Get()->RegisterAssetHandler<T1Data>();
        AssetManager::Get()->RegisterAssetHandler<T2Data>();
        AssetManager::Get()->RegisterAssetHandler<T3Data>();
    }

    static void TearDownTestSuite()
    {
    }
};

TEST_F(AssetManagerTest, BuilderTest)
{
    auto *db = AssetDataBase::Get();
    db->Load();
    auto src = db->RegisterAsset("framework/data/test_asset.t3");
    db->Save();
    db->Dump(std::cout);

    auto asset = AssetManager::Get()->LoadAsset<T3Data>(src->uuid);
    ASSERT_NE(asset, nullptr);
    asset->BlockUntilLoaded();

    auto &data = asset->Data();
    auto t1 = AssetManager::Get()->FindAsset<T1Data>(data.t1);
    auto t2 = AssetManager::Get()->FindAsset<T2Data>(data.t2);


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

TEST_F(AssetManagerTest, MutationTest)
{
    auto *db = AssetDataBase::Get();

    const std::string src = "framework/data/mut_tmp.t3";
    const std::string moved = "framework/data/mut_moved.t3";
    const std::string dup = "framework/data/mut_dup.t3";

    auto writeFile = [db](const std::string &path) {
        auto file = db->CreateOrOpenFile(FilePath{ FilePath(path) });
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
    auto movedAsset = db->MoveAsset(FilePath{ FilePath(src) }, FilePath{ FilePath(moved) });
    ASSERT_NE(movedAsset, nullptr);
    EXPECT_TRUE(movedAsset->uuid == original);

    // Duplicate assigns a new identity.
    auto dupAsset = db->DuplicateAsset(FilePath{ FilePath(moved) }, FilePath{ FilePath(dup) });
    ASSERT_NE(dupAsset, nullptr);
    EXPECT_FALSE(dupAsset->uuid == original);

    // Remove drops identity.
    db->RemoveAsset(original);
    EXPECT_EQ(db->FindAsset(original), nullptr);

    // Cleanup generated sources and the manifest written by this test.
    auto root = db->GetWorkSpaceFs()->GetPath().GetStr();
    for (const auto &name : { src, moved, dup }) {
        std::filesystem::remove(FilePath(root + "/" + name).GetStr());
    }
    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}

TEST_F(AssetManagerTest, ManifestIdentityTest)
{
    auto *db = AssetDataBase::Get();

    const std::string path = "framework/data/manifest_seed.t3";
    const Uuid seeded = Uuid::Create();

    // Pre-write a manifest entry; registration must reuse it regardless of the path.
    auto fs = db->GetWorkSpaceFs();
    AssetIndexFile manifest("file", "cook");
    IndexFileEntry entry;
    entry.key = "manifest_seed.t3";
    entry.id = seeded;
    manifest.Set(entry);
    AssetIndexFile::Save(fs, FilePath("framework/data/assets.jsonl"), manifest);
    db->Reset(); // drop cached maps + parsed manifests so the seeded manifest is reloaded

    auto file = db->CreateOrOpenFile(FilePath{ FilePath(path) });
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
    auto *db = AssetDataBase::Get();
    const std::string path = "framework/data/ondemand.t1";

    auto file = db->CreateOrOpenFile(FilePath{ FilePath(path) });
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

TEST_F(AssetManagerTest, InProcessCookRunnerTest)
{
    auto *db = AssetDataBase::Get();
    const std::string path = "framework/data/ondemand_runner.t1";

    auto file = db->CreateOrOpenFile(FilePath{ FilePath(path) });
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
    db->RebuildCacheFromScan();

    EXPECT_NE(db->FindAsset("framework/data/test_asset.t1"), nullptr);
    EXPECT_NE(db->FindAsset("framework/data/test_asset.t2"), nullptr);
    EXPECT_NE(db->FindAsset("framework/data/test_asset.t3"), nullptr);
}

TEST_F(AssetManagerTest, ImportTest)
{
    auto *db = AssetDataBase::Get();
    auto root = db->GetWorkSpaceFs()->GetPath().GetStr();

    const FilePath sourceFile(root + "/framework/data/test_asset.t1");
    const FilePath dest{ FilePath("framework/data/imported.t1") };

    auto asset = db->ImportAsset(sourceFile, dest, false);
    ASSERT_NE(asset, nullptr);
    EXPECT_NE(db->FindAsset("framework/data/imported.t1"), nullptr);

    std::filesystem::remove(FilePath(root + "/framework/data/imported.t1").GetStr());
    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}

TEST_F(AssetManagerTest, CompressionTest)
{
    CompressionManager::Get()->Register(CompressionMethod::LZ4, new DummyCompressor());
    AssetManager::Get()->SetProductCompression(CompressionMethod::LZ4);

    auto *db = AssetDataBase::Get();
    const std::string path = "framework/data/compressed.t1";

    auto file = db->CreateOrOpenFile(FilePath{ FilePath(path) });
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
    auto file = db->CreateOrOpenFile(FilePath{ FilePath(path) });
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

TEST_F(AssetManagerTest, TypeIdentityTest)
{
    // The builder registry reports the same AssetTypeId as the compile-time trait.
    auto *builder = AssetBuilderManager::Get()->QueryBuilder(".t1");
    ASSERT_NE(builder, nullptr);
    EXPECT_EQ(std::string(builder->QueryType(".t1")), std::string(AssetTraits<T1Data>::ASSET_TYPE));

    // The source-derived type matches.
    auto *db = AssetDataBase::Get();
    auto root = db->GetWorkSpaceFs()->GetPath().GetStr();
    const std::string path = "framework/data/type_id.t1";

    auto file = db->CreateOrOpenFile(FilePath{ FilePath(path) });
    ASSERT_NE(file, nullptr);
    {
        auto archive = file->WriteAsArchive();
        const char data[] = "{\"val\": 1}";
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
    auto *db = AssetDataBase::Get();
    auto root = db->GetWorkSpaceFs()->GetPath().GetStr();
    const std::string path = "framework/data/concurrent.t1";

    auto file = db->CreateOrOpenFile(FilePath{ FilePath(path) });
    ASSERT_NE(file, nullptr);
    {
        auto archive = file->WriteAsArchive();
        const char data[] = "{\"val\": 9}";
        archive->SaveRaw(data, sizeof(data) - 1);
        archive->Flush();
    }
    file = nullptr;

    auto src = db->RegisterAsset(path, false);
    ASSERT_NE(src, nullptr);

    // Concurrent loads of an unbuilt asset must coalesce onto one in-process cook without deadlock.
    std::vector<std::shared_ptr<Asset<T1Data>>> results(4);
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([src, i, &results]() {
            results[i] = AssetManager::Get()->LoadAsset<T1Data>(src->uuid);
        });
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

TEST_F(AssetManagerTest, CookWorkerTest)
{
    auto *db = AssetDataBase::Get();
    auto root = db->GetWorkSpaceFs()->GetPath().GetStr();
    const std::string path = "framework/data/batch.t1";

    auto file = db->CreateOrOpenFile(FilePath{ FilePath(path) });
    ASSERT_NE(file, nullptr);
    {
        auto archive = file->WriteAsArchive();
        const char data[] = "{\"val\": 3}";
        archive->SaveRaw(data, sizeof(data) - 1);
        archive->Flush();
    }
    file = nullptr;

    auto src = db->RegisterAsset(path, false);
    ASSERT_NE(src, nullptr);

    // Batch cook the work list, then load the freshly produced product.
    CookWorker worker;
    worker.CookBatch({ CookWorker::Job{ src->uuid, "common" } });

    auto asset = AssetManager::Get()->LoadAsset<T1Data>(src->uuid);
    ASSERT_NE(asset, nullptr);
    asset->BlockUntilLoaded();
    EXPECT_TRUE(asset->IsLoaded());
    EXPECT_EQ(asset->Data().v, 3);

    std::filesystem::remove(FilePath(root + "/" + path).GetStr());
    std::filesystem::remove(FilePath(root + "/framework/data/assets.jsonl").GetStr());
}