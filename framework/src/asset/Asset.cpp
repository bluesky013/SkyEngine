//
// Created by yjrj on 2022/8/8.
//

#include <framework/asset/Asset.h>

namespace sky {

    void AssetBase::SetUuid(const Uuid& id)
    {
        uuid = id;
    }

    const Uuid& AssetBase::GetUuid() const
    {
        return uuid;
    }

}