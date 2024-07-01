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

using namespace sky;

struct T1Data {
    int v;
};

struct T2Data {
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

struct T3Data {
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
        if (request.assetInfo->type == AssetTraits<T1Data>::ASSET_TYPE) {
            auto asset = std::static_pointer_cast<Asset<T1Data>>(am->FindOrCreateAsset(request.assetInfo->uuid, "T1"));

            T1Data &data = asset->Data();
            json.Start("val");
            data.v = json.LoadInt();

            am->SaveAsset(asset, "common");
        } else {
            auto asset = std::static_pointer_cast<Asset<T2Data>>(am->FindOrCreateAsset(request.assetInfo->uuid, "T2"));

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
        auto asset = std::static_pointer_cast<Asset<T3Data>>(am->FindOrCreateAsset(request.assetInfo->uuid, "T3"));

        auto p1Asset = AssetDataBase::Get()->ImportAsset(p1);
        auto p2Asset = AssetDataBase::Get()->ImportAsset(p2);
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
        AssetDataBase::Get()->SetWorkSpaceFs(projectFs);

        AssetBuilderManager::Get()->RegisterBuilder(new TestBuilder1());
        AssetBuilderManager::Get()->RegisterBuilder(new TestBuilder2());

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
    auto src = db->ImportAsset("framework/data/test_asset.t3");
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
}