//
// Created by Zach Lee on 2021/12/21.
//

#include <framework/asset/Asset.h>
#include <framework/asset/AssetManager.h>

namespace sky {

    void AssetBase::OnExpire()
    {
        AssetManager::Get()->DestroyAsset(uuid);
    }

}