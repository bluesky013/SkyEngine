//
// Created by Zach Lee on 2021/11/12.
//


#pragma once
#include <framework/interface/EngineInterface.h>
#include <framework/environment/Singleton.h>
#include <framework/window/IWindowEvent.h>
#include <vector>
#include <memory>

namespace sky {

    class Render;
    class World;
    class Viewport;

    struct IEngineEvent {
        virtual void OnAddWorld(World&) {}

        virtual void OnRemoveWorld(World&) {}

        virtual void OnAddViewport(Viewport&) {}

        virtual void OnRemoveViewport(Viewport&) {}

        virtual void OnWorldTargetChange(World& world, Viewport& vp) {}

        virtual void OnTick(float time) {}

        virtual void OnWindowResize(void* hwnd, uint32_t, uint32_t) {}
    };

    class SkyEngine
        : public IEngine
        , public IWindowEvent
        , public Singleton<SkyEngine> {
    public:
        virtual bool Init(const StartInfo&) override;

        virtual void Tick(float) override;

        virtual void DeInit() override;

        void AddWorld(World&);

        void RemoveWorld(World&);

        void AddViewport(Viewport&);

        void RemoveViewport(Viewport&);

        void SetTarget(World& world, Viewport& viewport);

        void RegisterEngineListener(IEngineEvent*);

        void UnRegisterEngineListener(IEngineEvent*);

        void RegisterModule(IModule*) override;

        void UnRegisterModule(IModule*) override;

        void OnWindowResize(void* window, uint32_t width, uint32_t height) override;

        IWindowEvent* GetEventHandler() override;

        static void Reflect();

    private:
        template <typename T>
        friend class Singleton;

        SkyEngine() = default;
        ~SkyEngine() = default;

        template <typename Func>
        void EachListener(Func&& f)
        {
            for(auto& listener : eventListeners) {
                f(listener);
            }
        }

        std::vector<World*> worlds;
        std::vector<Viewport*> viewports;
        std::vector<IModule*> modules;
        std::vector<IEngineEvent*> eventListeners;
        Render* render = nullptr;
    };

}