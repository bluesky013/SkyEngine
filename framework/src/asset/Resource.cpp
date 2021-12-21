//
// Created by Zach Lee on 2021/12/21.
//

#include <framework/asset/Resource.h>
#include <framework/asset/ResourceManager.h>

namespace sky {

    void ResourceBase::OnExpire()
    {
        ResourceManager::Get()->DestroyInstance(uuid);
    }

}