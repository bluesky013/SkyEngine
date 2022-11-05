//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindow.h>
#include "DXSampleBase.h"
#include <vector>
#include <functional>

namespace sky {

    class DX12Sample : public IModule, public IWindowEvent {
    public:
        DX12Sample() = default;
        ~DX12Sample() = default;

        void Init() override;

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;

        template <typename T>
        void RegisterSample()
        {
            samples.emplace_back([]() -> DX12SampleBase* {
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
        std::vector<std::function<DX12SampleBase*()>> samples;
        uint32_t currentIndex = 0;
        std::unique_ptr<DX12SampleBase> currentSample;
    };

}