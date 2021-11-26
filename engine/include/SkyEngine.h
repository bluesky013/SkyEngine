//
// Created by Zach Lee on 2021/11/12.
//


#pragma once
#include <application/interface/EngineInterface.h>
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
    };

    class SkyEngine : public IEngine {
    public:
        SkyEngine() = default;
        ~SkyEngine() = default;

        virtual bool Init(const StartInfo&) override;

        virtual void Tick(float) override;

        virtual void DeInit() override;

        void AddWorld(World&);

        void RemoveWorld(World&);

        void AddViewport(Viewport&);

        void RemoveViewport(Viewport&);

        void RegisterEngineListener(IEngineEvent*);

        void UnRegisterEngineListener(IEngineEvent*);

        void RegisterModule(IModule*) override;

        void UnRegisterModule(IModule*) override;

    private:
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