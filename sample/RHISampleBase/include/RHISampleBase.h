//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindow.h>
#include <rhi/Device.h>
#include <rhi/Instance.h>
#include <rhi/Swapchain.h>

namespace sky::rhi {
    class NativeWindow;

    class RHISampleBase : public IWindowEvent {
    public:
        RHISampleBase()  = default;
        ~RHISampleBase() = default;

        virtual void OnStart();
        virtual void OnStop();
        virtual void OnTick(float delta);

    protected:
        void OnWindowResize(uint32_t width, uint32_t height) override;
        rhi::Instance *instance = nullptr;
        rhi::Device *device = nullptr;

        rhi::SwapChainPtr swapChain;

        uint32_t frameIndex = 0;
        uint32_t frame = 0;
    };
}
