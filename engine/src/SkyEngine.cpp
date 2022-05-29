//
// Created by Zach Lee on 2021/11/11.
//

#include <engine/SkyEngine.h>
#include <engine/world/World.h>
#include <core/logger/Logger.h>

static const char* TAG = "SkyEngine";

namespace sky {

    bool SkyEngine::Init(const StartInfo& startInfo)
    {
        return true;
    }

    void SkyEngine::Tick(float time)
    {
        for (auto& world : worlds) {
            world->Tick(time);
        }

        EachListener([time](IEngineEvent* event) {
            event->OnTick(time);
        });
    }

    void SkyEngine::DeInit()
    {
        eventListeners.clear();
    }

    void SkyEngine::AddWorld(World& world)
    {
        auto iter = std::find(worlds.begin(), worlds.end(), &world);
        if (iter != worlds.end()) {
            return;
        }
        worlds.emplace_back(&world);
        EachListener([&world](IEngineEvent* event) {
            event->OnAddWorld(world);
        });
    }

    void SkyEngine::RemoveWorld(World& world)
    {
        auto iter = std::find(worlds.begin(), worlds.end(), &world);
        if (iter != worlds.end()) {
            worlds.erase(iter);
        }
        EachListener([&world](IEngineEvent* event) {
            event->OnRemoveWorld(world);
        });
    }

    void SkyEngine::AddViewport(Viewport& vp)
    {
        auto iter = std::find(viewports.begin(), viewports.end(), &vp);
        if (iter != viewports.end()) {
            return;
        }
        viewports.emplace_back(&vp);
        EachListener([&vp](IEngineEvent* event) {
            event->OnAddViewport(vp);
        });
    }

    void SkyEngine::RemoveViewport(Viewport& vp)
    {
        auto iter = std::find(viewports.begin(), viewports.end(), &vp);
        if (iter != viewports.end()) {
            viewports.erase(iter);
        }
        EachListener([&vp](IEngineEvent* event) {
            event->OnRemoveViewport(vp);
        });
    }

    void SkyEngine::SetTarget(World& world, Viewport& viewport)
    {
        EachListener([&world, &viewport](IEngineEvent* event) {
            event->OnWorldTargetChange(world, viewport);
        });
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

    void SkyEngine::OnWindowResize(void* hwnd, uint32_t w, uint32_t h)
    {
        EachListener([w, h, hwnd](IEngineEvent* event) {
            event->OnWindowResize(hwnd, w, h);
        });
        LOG_I(TAG, "window resize %u, %u", w, h);
    }

    IWindowEvent* SkyEngine::GetEventHandler()
    {
        return this;
    }

}