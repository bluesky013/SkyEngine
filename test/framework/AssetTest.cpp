//
// Created by Zach Lee on 2022/8/11.
//

#include <core/file/FileIO.h>
#include <framework/asset/AssetManager.h>
#include <framework/database/DataBase.h>
#include <framework/database/DBManager.h>
#include <gtest/gtest.h>

struct Test1Data {
    int         a;
    float       b;
    std::string c;

    template <class Archive>
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
        using DataType                                = Test1Data;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("5F34BBB0-3E06-4197-B1A9-069C18D5D3C5");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::JSON;

        static std::shared_ptr<Test1> CreateFromData(const DataType &data)
        {
            auto res   = std::make_shared<Test1>();
            res->value = data;
            return res;
        }
    };
} // namespace sky

class AssetTest : public ::testing::Test {
public:
    static void SetUpTestSuite()
    {
        sky::DBManager::Get()->Init();
    }

    static void TearDownTestSuite()
    {
        sky::AssetManager::Destroy();
        sky::DBManager::Destroy();
    }

    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

TEST_F(AssetTest, DataTest)
{
    const char *CREATE = "CREATE TABLE IF NOT EXISTS data("
                             "key   TEXT PRIMARY KEY,"
                             "value TEXT);";
    const char *UPDATE = "REPLACE INTO data(key, value) VALUES (?,?);";
    const char *SELECT = "SELECT value FROM data WHERE key=?;";

    {
        sky::DataBase dataBase;
        dataBase.Init("test.db");

        {
            std::unique_ptr<sky::db::Statement> tableStmt(dataBase.CreateStatement(CREATE));
            tableStmt->Step();
            tableStmt->Finalize();
        }

        {
            std::unique_ptr<sky::db::Statement> addStmt(dataBase.CreateStatement(UPDATE));
            addStmt->BindText(1, "key");
            addStmt->BindText(2, "value");
            addStmt->Step();
            addStmt->Finalize();
        }

        dataBase.Shutdown();
    }

    {
        sky::DataBase dataBase;
        dataBase.Init("test.db");

        std::unique_ptr<sky::db::Statement> selectStmt(dataBase.CreateStatement(SELECT));
        selectStmt->BindText(1, "key");
        selectStmt->Step();
        std::string val = selectStmt->GetText(0);
        ASSERT_EQ(val, "value");
    }

}
