//
// Created by Zach Lee on 2023/2/20.
//

#pragma once

#include <framework/database/DataBase.h>
#include <core/util/Uuid.h>
#include <memory>

namespace sky {

    class AssetDataBase {
    public:
        AssetDataBase() = default;
        ~AssetDataBase();

        void Init(const std::string &name);

        void AddSource(const std::string &path, const std::string &folder);
        bool HasSource(const std::string &path) const;

        void AddProduct(const Uuid &uuid, const std::string &path);

    private:
        std::unique_ptr<DataBase> dataBase;

        // source table
        std::unique_ptr<db::Statement> createSourceTableStat;
        std::unique_ptr<db::Statement> insertSourceTableStat;
        std::unique_ptr<db::Statement> selectSourceTableStat;

        // product table
        std::unique_ptr<db::Statement> createProductTableStat;
        std::unique_ptr<db::Statement> insertProductTableStat;
    };

}