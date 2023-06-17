//
// Created by Zach Lee on 2023/2/20.
//

#pragma once

#include <framework/database/DataBase.h>
#include <core/util/Uuid.h>
#include <memory>

namespace sky {

    struct SourceData {
        std::string path;
        std::string folder;
        std::string productKey;
        Uuid uuid;
    };

    struct ProductData {
        Uuid uuid;
        std::string path;
    };

    class AssetDataBase {
    public:
        AssetDataBase() = default;
        ~AssetDataBase();

        void Init(const std::string &name);

        void AddSource(const SourceData &sourceData);
        bool QueryProduct(const std::string &sourcePath, const std::string &key, Uuid &uuid) const;
        bool QueryProduct(const Uuid &uuid, std::string &out);

        void AddProduct(const ProductData &productData);

    private:
        std::unique_ptr<DataBase> dataBase;

        // source table
        std::unique_ptr<db::Statement> createSourceTableStat;
        std::unique_ptr<db::Statement> insertSourceTableStat;
        std::unique_ptr<db::Statement> selectSourceTableStat;

        // product table
        std::unique_ptr<db::Statement> createProductTableStat;
        std::unique_ptr<db::Statement> insertProductTableStat;
        std::unique_ptr<db::Statement> selectProductTableStat;
    };

}