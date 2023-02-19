//
// Created by Zach Lee on 2023/2/20.
//

#pragma once

#include <framework/database/DataBase.h>
#include <memory>

namespace sky {

    class AssetDataBase {
    public:
        AssetDataBase() = default;
        ~AssetDataBase() = default;

        void Init(const std::string &name);

    private:
        std::unique_ptr<DataBase> dataBase;
    };

}