//
// Created by Zach Lee on 2021/11/12.
//


#pragma once
#include <application/interface/EngineLoop.h>
#include <vector>

namespace sky {

    class Render;
    class World;
    class Viewport;

    struct IEngineEvent {
        void OnAddWorld(World*) {}
        void OnRemoveWorld(World*) {}

        void OnAddViewport(Viewport*) {}
        void OnRemoveViewport(Viewport*) {}
    };

    class SkyEngine : public IEngineLoop {
    public:
        SkyEngine() = default;
        ~SkyEngine() = default;

        virtual bool Init(const StartInfo&) override;

        virtual void Tick() override;

        virtual void DeInit() override;

        void AddWorld(World*);

        void RemoveWorld(World*);

        void AddViewport(Viewport*);

        void RemoveViewport(Viewport*);

        void RegisterEngineListener(IEngineEvent*);

        void UnRegisterEngineListener(IEngineEvent*);

    private:
        std::vector<World*> worlds;
        std::vector<Viewport*> viewports;
        std::vector<IEngineEvent*> eventListeners;
        Render* render = nullptr;
    };

}