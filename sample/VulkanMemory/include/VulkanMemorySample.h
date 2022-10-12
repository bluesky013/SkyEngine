//
// Created by Zach Lee on 2022/6/16.
//

#pragma once

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/NativeWindow.h>

namespace sky {
    class NativeWindow;

    class VulkanMemorySample : public IModule {
    public:
        VulkanMemorySample()  = default;
        ~VulkanMemorySample() = default;

        void Init() override;

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;

    private:
    };

} // namespace sky