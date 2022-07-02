//
// Created by Zach Lee on 2022/6/16.
//


#pragma once

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/NativeWindow.h>
#include <render/Render.h>
#include <render/RenderScene.h>
#include <render/RenderViewport.h>

namespace sky::render {
    class NativeWindow;

    class FrameGraphSample : public IModule {
    public:
        FrameGraphSample() = default;
        ~FrameGraphSample() = default;

        void Init() override;

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;

    private:
        RDViewportPtr viewport;
        drv::Device* device = nullptr;
        drv::SemaphorePtr imageAvailable;
        drv::SemaphorePtr renderFinish;
        drv::ImagePtr depthStencil;
    };

}