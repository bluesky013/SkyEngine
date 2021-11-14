//
// Created by Zach Lee on 2021/11/11.
//

#include <SkyEngine.h>
#include <core/platform/Platform.h>
#include <world/World.h>
#include <render/Render.h>

namespace sky {

    bool SkyEngine::Init(const StartInfo& startInfo)
    {
        render = new Render();
        render->Init(startInfo);
        RegisterEngineListener(render);

        return true;
    }

    void SkyEngine::Tick()
    {
        for (auto& world : worlds) {
            world->Tick();
        }
    }

    void SkyEngine::DeInit()
    {
        eventListeners.clear();

        if (render != nullptr) {
            delete render;
            render = nullptr;
        }
    }

    void SkyEngine::AddWorld(World* world)
    {
        if (world != nullptr) {
            worlds.emplace_back(world);
        }
    }

    void SkyEngine::RemoveWorld(World* world)
    {
        auto iter = std::find(worlds.begin(), worlds.end(), world);
        if (iter != worlds.end()) {
            worlds.erase(iter);
        }
    }

    void SkyEngine::AddViewport(Viewport* vp)
    {
        if (vp != nullptr) {
            viewports.emplace_back(vp);
        }
    }

    void SkyEngine::RemoveViewport(Viewport* vp)
    {
        auto iter = std::find(viewports.begin(), viewports.end(), vp);
        if (iter != viewports.end()) {
            viewports.erase(iter);
        }
    }

    void SkyEngine::RegisterEngineListener(IEngineEvent* event)
    {
        if (event != nullptr) {
            eventListeners.emplace_back(event);
        }
    }

    void SkyEngine::UnRegisterEngineListener(IEngineEvent* event)
    {
        auto iter = std::find(eventListeners.begin(), eventListeners.end(), event);
        if (iter != eventListeners.end()) {
            eventListeners.erase(iter);
        }
    }

}

extern "C" SKY_EXPORT sky::IEngineLoop* StartEngine()
{
    return new sky::SkyEngine();
}

extern "C" SKY_EXPORT void ShutdownEngine(sky::IEngineLoop* engine)
{
    if (engine != nullptr) {
        delete engine;
    }
}