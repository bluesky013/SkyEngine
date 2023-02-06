//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindow.h>
#include "VulkanSampleBase.h"
#include <vector>

namespace sky {

    class VulkanSample : public IModule, public IWindowEvent {
    public:
        VulkanSample() = default;
        ~VulkanSample() = default;

        bool Init() override;

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;

        template <typename T>
        void RegisterSample()
        {
            samples.emplace_back([]() -> VulkanSampleBase* {
                auto *sample =  new T();
                sample->OnStart();
                return sample;
            });
        }

    private:
        void StartSample();
        void NextSample();
        void PrevSample();

        void OnKeyUp(KeyButtonType) override;

        std::vector<std::function<VulkanSampleBase*()>> samples;
        uint32_t currentIndex = 0;
        std::unique_ptr<VulkanSampleBase> currentSample;
    };

}
