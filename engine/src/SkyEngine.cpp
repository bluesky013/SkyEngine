//
// Created by Zach Lee on 2021/11/11.
//

#include <core/logger/Logger.h>
#include <engine/SkyEngine.h>
#include <engine/world/World.h>

static const char *TAG = "SkyEngine";

namespace sky {

    bool SkyEngine::Init(const StartInfo &startInfo)
    {
        return true;
    }

    void SkyEngine::Tick(float time)
    {
    }

    void SkyEngine::DeInit()
    {
    }

} // namespace sky