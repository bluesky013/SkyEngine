
//
// Created by Zach Lee on 2021/11/14.
//


#pragma once

#include <engine/world/World.h>

namespace sky {

    namespace drv {
        class SwapChain;
    }

    class Render;

    class RenderScene {
    public:
        RenderScene(Render& rd) : render(rd) {}
        ~RenderScene() = default;

        void SetTarget(drv::SwapChain& swc);

    private:
        Render& render;
        drv::SwapChain* swapChain = nullptr;
    };

}