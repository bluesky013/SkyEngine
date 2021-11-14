//
// Created by Zach Lee on 2021/11/12.
//


#pragma once
#include <SkyEngine.h>
#include <unordered_map>

namespace sky {

    namespace drv {
        class Driver;
        class Device;
        class SwapChain;
    }

    class Render : public IEngineEvent {
    public:
        Render() = default;
        ~Render();

        bool Init(const StartInfo&);

        void OnAddWorld(World*);
        void OnRemoveWorld(World*);

        void OnAddViewport(Viewport*);
        void OnRemoveViewport(Viewport*);

    private:
        drv::Driver* driver = nullptr;
        drv::Device* device = nullptr;
        std::unordered_map<Viewport*, drv::SwapChain*> swapChains;
    };

}