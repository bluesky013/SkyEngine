//
// Created by Zach Lee on 2021/12/4.
//

#include <gtest/gtest.h>
#include <framework/asset/AssetManager.h>
#include <core/logger/Logger.h>
static const char* TAG = "AssetTest";

using namespace sky;

uint32_t g_Counter = 0;

struct TestAssetSourceData {
    uint32_t v1;
    uint32_t v2;

    template <class Archive>
    void serialize(Archive& ar)
    {
        ar(v1, v2);
    }
};

class TestAsset : public AssetBase {
public:
    TestAsset(const Uuid& id) : AssetBase(id)
    {
        g_Counter++;
    }

    ~TestAsset()
    {
        g_Counter--;
    }

    static constexpr Uuid TYPE = Uuid::CreateFromString("7f0655e6-6177-4a16-970f-4a09e7d6a9b7");

    const Uuid& GetType() const override
    {
        return TYPE;
    }

    ResourceInstance CreateInstance(const Uuid&) override
    {
        return ResourceInstance {};
    }

    template<class Archive>
    void load(Archive& ar)
    {
        ar(source);
    }

    template<class Archive>
    void save(Archive& ar) const
    {
        ar(source);
    }

    TestAssetSourceData source;
};

TEST(AssetTest, AssetManagerSingleton)
{
    auto mgr = AssetManager::Get();
    ASSERT_NE(mgr, nullptr);

    mgr->RegisterHandler<TestAsset>();

    g_Counter = 0;
    {
        auto asset = mgr->FindOrCreate<TestAsset>(Uuid::Create());
        ASSERT_EQ(g_Counter, 1);
    }

    ASSERT_EQ(g_Counter, 0);

    AssetManager::Destroy();
}

TEST(AssetTest, AssetManagerCreate)
{
    auto mgr = AssetManager::Get();
    ASSERT_NE(mgr, nullptr);

    mgr->RegisterHandler<TestAsset>();

    auto id = Uuid::Create();
    {
        CounterPtr<TestAsset> asset = mgr->FindOrCreate<TestAsset>(id);
        asset->source.v1 = 1;
        asset->source.v2 = 2;
        mgr->SaveAsset("Test.asset", asset, TestAsset::TYPE);
    }
    {
        auto asset = Cast<TestAsset>(mgr->LoadAsset("Test.asset", TestAsset::TYPE));
        ASSERT_EQ(!!asset, true);
        ASSERT_EQ(asset->source.v1, 1);
        ASSERT_EQ(asset->source.v2, 2);
    }

    AssetManager::Destroy();
}