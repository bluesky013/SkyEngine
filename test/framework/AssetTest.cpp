//
// Created by Zach Lee on 2021/12/4.
//

#include <gtest/gtest.h>
#include <framework/asset/AssetManager.h>

using namespace sky;

uint32_t g_Counter = 0;

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
};

class TestAssetHandler : public AssetHandlerBase {
public:
    TestAssetHandler() = default;
    ~TestAssetHandler() = default;

    AssetBase* Create(const Uuid& id) override
    {
        return new TestAsset(id);
    }

    AssetBase* Load(const std::string&) override
    {
        return new TestAsset(Uuid::Create());
    }
};

TEST(AssetTest, AssetManagerSingleton)
{
    auto mgr = AssetManager::Get();
    ASSERT_NE(mgr, nullptr);

    mgr->RegisterHandler<TestAsset>(new TestAssetHandler());

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
}