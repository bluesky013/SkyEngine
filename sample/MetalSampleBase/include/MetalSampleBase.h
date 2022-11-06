//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindow.h>
#include <metal/Instance.h>
#include <metal/Device.h>

namespace sky {
    class NativeWindow;

    class MetalSampleBase : public IModule, public IWindowEvent {
    public:
        MetalSampleBase()  = default;
        ~MetalSampleBase() = default;

        void Init() override;

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;

        void OnWindowResize(uint32_t width, uint32_t height) override;

        virtual void OnStart() {}
        virtual void OnStop() {}

    protected:
        mtl::Instance *instance = nullptr;
        mtl::Device *device = nullptr;

        uint32_t frameIndex = 0;
        uint32_t frame = 0;
    };
}
