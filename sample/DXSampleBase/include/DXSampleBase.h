//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindow.h>
#include <dx12/Device.h>
#include <dx12/Instance.h>

namespace sky {
    class NativeWindow;

    class DX12SampleBase : public IWindowEvent {
    public:
        DX12SampleBase()  = default;
        ~DX12SampleBase() = default;

        virtual void OnStart();
        virtual void OnStop();
        virtual void OnTick(float delta);

    protected:
        void OnWindowResize(uint32_t width, uint32_t height) override;
        dx::Instance *instance = nullptr;
        dx::Device *device = nullptr;
        uint32_t frameIndex = 0;
        uint32_t frame = 0;
    };
}