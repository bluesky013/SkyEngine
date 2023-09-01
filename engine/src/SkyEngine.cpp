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
        return true;
    }

    void SkyEngine::Tick(float time)
    {
        for (const auto &world : worlds) {
            world->Tick(time);
        }
    }

    void SkyEngine::DeInit()
    {
        AssetManager::Destroy();
    }

    void SkyEngine::AddWorld(const WorldPtr& world)
    {
        worlds.emplace(world);
    }

    void SkyEngine::RemoveWorld(const WorldPtr& world)
    {
        worlds.erase(world);
    }
} // namespace sky