//
// Created by yjrj on 2022/8/8.
//

#include <framework/asset/Asset.h>

namespace sky {

    void Asset::SetUuid(const Uuid& id)
    {
        uuid = id;
    }

    const Uuid& Asset::GetUuid() const
    {
        return uuid;
    }

}