//
// Created by Zach Lee on 2022/8/11.
//

#include <gtest/gtest.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/BasicSerialization.h>
#include <core/file/FileIO.h>

struct Test1Data {
    int a;
    float b;
    std::string c;

    template<class Archive>
    void serialize(Archive &ar)
    {
        ar(a, b, c);
    }
};

struct Test1 {
    Test1Data value;
};

namespace sky {
    template <>
    struct AssetTraits<Test1> {
        using DataType = Test1Data;
        static constexpr Uuid ASSET_TYPE = Uuid::CreateFromString("5F34BBB0-3E06-4197-B1A9-069C18D5D3C5");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::JSON;

        static std::shared_ptr<Test1> CreateFromData(const DataType& data)
        {
            auto res = std::make_shared<Test1>();
            res->value = data;
            return res;
        }
    };
}

class AssetTest : public ::testing::Test {
public:
    static void SetUpTestSuite()
    {
        auto am = sky::AssetManager::Get();
        am->RegisterAssetHandler<Test1>();

        auto testAsset = std::make_shared<sky::Asset<Test1>>();
        testAsset->Data().a = 1;
        testAsset->Data().b = 2.0;
        testAsset->Data().c = "abc";
        am->SaveAsset(testAsset, "test\\framework\\t1.json");
    }

    static void TearDownTestSuite()
    {
        sky::AssetManager::Destroy();
    }

    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

TEST_F(AssetTest, LoadTest1)
{
    auto asset = sky::AssetManager::Get()->LoadAsset<Test1>("test\\framework\\t1.json");
    ASSERT_EQ(asset->GetStatus(), sky::AssetBase::Status::LOADED);
    asset->BlockUtilLoaded();
    asset->BlockUtilLoaded();
    ASSERT_EQ(asset->GetStatus(), sky::AssetBase::Status::LOADED);
    auto instance = asset->CreateInstance();
    ASSERT_EQ(instance->value.a, 1);
    ASSERT_EQ(instance->value.b, 2.f);
    ASSERT_EQ(instance->value.c, "abc");
}

TEST_F(AssetTest, AsyncLoadTest1)
{
    auto asset = sky::AssetManager::Get()->LoadAsset<Test1>("test\\framework\\t1.json", true);
    ASSERT_EQ(asset->GetStatus(), sky::AssetBase::Status::LOADING);
    asset->BlockUtilLoaded();
    ASSERT_EQ(asset->GetStatus(), sky::AssetBase::Status::LOADED);
    auto instance = asset->CreateInstance();
    ASSERT_EQ(instance->value.a, 1);
    ASSERT_EQ(instance->value.b, 2.f);
    ASSERT_EQ(instance->value.c, "abc");
}