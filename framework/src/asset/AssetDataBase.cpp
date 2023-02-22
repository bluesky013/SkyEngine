//
// Created by Zach Lee on 2023/2/20.
//

#include <framework/asset/AssetDataBase.h>

namespace sky {

    static const char *CREATE_SOURCE_ASSET_TABLE = "CREATE TABLE IF NOT EXISTS SourceTable ( "
                                                   "    Path    TEXT PRIMARY KEY, "
                                                   "    Folder  TEXT NOT NULL COLLATE NOCASE);";

    static const char *INSERT_SOURCE = "INSERT OR REPLACE INTO SourceTable (Path, Folder) VALUES (:path, :folder);";


    static const char *CREATE_PRODUCT_ASSET_TABLE = "CREATE TABLE IF NOT EXISTS ProductTable ("
                                                    "    Uuid   TEXT PRIMARY KEY, "
                                                    "    Path   TEXT NOT NULL COLLATE NOCASE);";

    static const char *INSERT_PRODUCT = "INSERT OR REPLACE INTO ProductTable (Uuid, Path) VALUES (:uuid, :path);";

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
        }

        {
            createProductTableStat.reset(dataBase->CreateStatement(CREATE_PRODUCT_ASSET_TABLE));
            createProductTableStat->Step();
            createProductTableStat->Reset();

            insertProductTableStat.reset(dataBase->CreateStatement(INSERT_PRODUCT));
        }

    }

    void AssetDataBase::AddSource(const std::string &path, const std::string &folder)
    {
        insertSourceTableStat->BindText(1, path);
        insertSourceTableStat->BindText(2, folder);
        insertSourceTableStat->Step();
        insertSourceTableStat->Reset();
    }

    void AssetDataBase::AddProduct(const Uuid &uuid, const std::string &path)
    {
        insertProductTableStat->BindText(1, uuid.ToString());
        insertProductTableStat->BindText(2, path);
        insertProductTableStat->Step();
        insertProductTableStat->Reset();
    }

} // namespace sky