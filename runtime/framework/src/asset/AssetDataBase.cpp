//
// Created by Zach Lee on 2023/2/20.
//

#include <framework/asset/AssetDataBase.h>
#include <sqlite/sqlite3.h>

namespace sky {

    static const char *CREATE_SOURCE_ASSET_TABLE = "CREATE TABLE IF NOT EXISTS SourceTable ( "
                                                   "    Path        TEXT PRIMARY KEY COLLATE NOCASE, "
                                                   "    Folder      TEXT NOT NULL COLLATE NOCASE, "
                                                   "    ProductKey  TEXT KEY COLLATE NOCASE, "
                                                   "    Uuid        TEXT NOT NULL COLLATE NOCASE);";

    static const char *INSERT_SOURCE = "INSERT OR REPLACE INTO SourceTable (Path, Folder, ProductKey, Uuid) VALUES (:path, :folder, :pKey, :uuid);";
    static const char *SELECT_SOURCE = "SELECT * FROM SourceTable WHERE Path = :path AND ProductKey = :pKey;";

    static const char *CREATE_PRODUCT_ASSET_TABLE = "CREATE TABLE IF NOT EXISTS ProductTable ("
                                                    "    Uuid   TEXT PRIMARY KEY, "
                                                    "    Path   TEXT NOT NULL COLLATE NOCASE);";

    static const char *INSERT_PRODUCT = "INSERT OR REPLACE INTO ProductTable (Uuid, Path) VALUES (:uuid, :path);";
    static const char *SELECT_PRODUCT = "SELECT * FROM ProductTable WHERE Uuid = :uuid;";

    AssetDataBase::~AssetDataBase()
    {
        createSourceTableStat = nullptr;
        insertSourceTableStat = nullptr;

        dataBase = nullptr;
    }

    void AssetDataBase::Init(const std::string &name)
    {
        dataBase = std::make_unique<DataBase>();
        dataBase->Init(name);

        {
            createSourceTableStat.reset(dataBase->CreateStatement(CREATE_SOURCE_ASSET_TABLE));
            createSourceTableStat->Step();
            createSourceTableStat->Reset();

            insertSourceTableStat.reset(dataBase->CreateStatement(INSERT_SOURCE));
            selectSourceTableStat.reset(dataBase->CreateStatement(SELECT_SOURCE));
        }

        {
            createProductTableStat.reset(dataBase->CreateStatement(CREATE_PRODUCT_ASSET_TABLE));
            createProductTableStat->Step();
            createProductTableStat->Reset();

            insertProductTableStat.reset(dataBase->CreateStatement(INSERT_PRODUCT));
            selectProductTableStat.reset(dataBase->CreateStatement(SELECT_PRODUCT));
        }
    }

    void AssetDataBase::AddSource(const SourceData &sourceData)
    {
        insertSourceTableStat->BindText(1, sourceData.path);
        insertSourceTableStat->BindText(2, sourceData.folder);
        insertSourceTableStat->BindText(3, sourceData.productKey);
        insertSourceTableStat->BindText(4, sourceData.uuid.ToString());
        insertSourceTableStat->Step();
        insertSourceTableStat->Reset();
    }

    bool AssetDataBase::QueryProduct(const std::string &sourcePath, const std::string &key, Uuid &uuid) const
    {
        selectSourceTableStat->BindText(1, sourcePath);
        selectSourceTableStat->BindText(2, key);

        selectSourceTableStat->Step();
        std::string source;
        source = selectSourceTableStat->GetText(3);

        selectSourceTableStat->Reset();
        if (source.empty()) {
            return false;
        }
        uuid = Uuid::CreateFromString(source);
        return true;
    }

    bool AssetDataBase::QueryProduct(const Uuid &uuid, std::string &out)
    {
        std::string idStr = uuid.ToString();
        selectProductTableStat->BindText(1, idStr);

        selectProductTableStat->Step();
        out = selectProductTableStat->GetText(1);
        selectProductTableStat->Reset();
        return !out.empty();
    }

    void AssetDataBase::AddProduct(const ProductData &productData)
    {
        insertProductTableStat->BindText(1, productData.uuid.ToString());
        insertProductTableStat->BindText(2,  productData.path);
        insertProductTableStat->Step();
        insertProductTableStat->Reset();
    }

} // namespace sky