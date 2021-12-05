//
// Created by Zach Lee on 2021/12/4.
//

#include <gtest/gtest.h>
#include <framework/asset/AssetManager.h>

using namespace sky;

class TestAsset : public AssetInstanceBase {
public:
    TestAsset() = default;
    ~TestAsset() = default;

    uint32_t GetType() const override
    {
        return TypeInfo<TestAsset>::Hash();
    }
};

TEST(AssetTest, AssetManagerSingleton)
{
    auto mgr1 = AssetManager::Get();
    ASSERT_NE(mgr1, nullptr);

    AssetManager::Destroy();
}

TEST(AssetTest, AssetManagerCreate)
{
}