//
// Created by Zach Lee on 2021/12/4.
//

#include <gtest/gtest.h>
#include <framework/asset/AssetManager.h>

using namespace sky;

uint32_t g_Counter = 0;

class TestAsset : public AssetInstanceBase {
public:
    TestAsset()
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
};

TEST(AssetTest, AssetManagerSingleton)
{
    auto mgr = AssetManager::Get();
    ASSERT_NE(mgr, nullptr);

    mgr->RegisterHandler<TestAsset>();

    g_Counter = 0;
    auto asset = mgr->CreateAsset<TestAsset>(Uuid::Create());
    ASSERT_NE(asset.Get(), nullptr);
    ASSERT_EQ(g_Counter, 1);

    mgr->DestroyAsset(asset.GetId());
    ASSERT_EQ(g_Counter, 0);

    AssetManager::Destroy();
}

TEST(AssetTest, AssetManagerCreate)
{
}