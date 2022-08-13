//
// Created by Zach Lee on 2022/8/11.
//

#include <gtest/gtest.h>
#include <framework/asset/AssetManager.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <core/file/FileIO.h>

static constexpr sky::Uuid T1 = sky::Uuid::CreateFromString("{EEA173FA-F7F1-4651-A008-A8F56CF6B393}");

struct Test1Data {
    int a;
    float b;
    std::string c;
};

struct Test1 {
    Test1Data value;
};

namespace sky {
    template <>
    struct AssetTraits<Test1> {
        using DataType = Test1Data;

        static void SaveToPath(const std::string& path, const DataType& data)
        {
        }

        static void LoadFromPath(const std::string& path, Test1Data& data)
        {
            std::string source;
            sky::ReadString(path, source);

            rapidjson::Document document;
            document.Parse(source.c_str());

            if (document.HasMember("test1")) {
                data.a = document["test1"].GetInt();
            }

            if (document.HasMember("test2")) {
                data.b = document["test2"].GetFloat();
            }

            if (document.HasMember("test3")) {
                data.c = document["test3"].GetString();
            }
        }

        static Test1* CreateFromData(const DataType& data)
        {
            auto res = new Test1();
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
        am->RegisterAsset(T1, "test/assets/t1.json");
    }

    static void TearDownTestSuite()
    {
        sky::AssetManager::Destroy();
    }

    void SetUp()
    {
    }

    void TearDown()
    {
    }
};

TEST_F(AssetTest, LoadTest1)
{
    auto asset = sky::AssetManager::Get()->LoadAsset<Test1>(T1);
    ASSERT_EQ(asset->GetStatus(), sky::AssetBase::Status::LOADED);
    asset->BlockUtilLoaded();
    asset->BlockUtilLoaded();
    ASSERT_EQ(asset->GetStatus(), sky::AssetBase::Status::LOADED);
    std::unique_ptr<Test1> instance(asset->CreateInstance());
    ASSERT_EQ(instance->value.a, 1);
    ASSERT_EQ(instance->value.b, 2.f);
    ASSERT_EQ(instance->value.c, "abc");
}

TEST_F(AssetTest, AsyncLoadTest1)
{
    auto asset = sky::AssetManager::Get()->LoadAssetAsync<Test1>(T1);
    ASSERT_EQ(asset->GetStatus(), sky::AssetBase::Status::LOADING);
    asset->BlockUtilLoaded();
    ASSERT_EQ(asset->GetStatus(), sky::AssetBase::Status::LOADED);
    std::unique_ptr<Test1> instance(asset->CreateInstance());
    ASSERT_EQ(instance->value.a, 1);
    ASSERT_EQ(instance->value.b, 2.f);
    ASSERT_EQ(instance->value.c, "abc");
}