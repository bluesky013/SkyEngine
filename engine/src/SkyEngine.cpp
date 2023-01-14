//
// Created by Zach Lee on 2021/11/11.
//

#include <engine/SkyEngine.h>
#include <framework/asset/AssetManager.h>
#include <framework/database/DBManager.h>

static const char *TAG = "SkyEngine";

namespace sky {

    bool SkyEngine::Init()
    {
        DBManager::Get()->Init();
        AssetManager::Get();

        return true;
    }

    void SkyEngine::Tick(float time)
    {
    }

    void SkyEngine::DeInit()
    {
        AssetManager::Destroy();
        DBManager::Destroy();
    }

} // namespace sky