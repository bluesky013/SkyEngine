//
// Created by Zach Lee on 2023/2/20.
//

#include <framework/asset/AssetDataBase.h>

namespace sky {

    void AssetDataBase::Init(const std::string &name)
    {
        dataBase = std::make_unique<DataBase>();
        dataBase->Init(name);
    }

} // namespace sky