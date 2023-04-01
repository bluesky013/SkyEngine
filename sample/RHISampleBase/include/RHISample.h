//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include "RHISampleBase.h"
#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindow.h>
#include <functional>
#include <vector>

namespace sky::rhi {

    class RHISample : public IModule, public IWindowEvent {
    public:
        RHISample() = default;
        ~RHISample() = default;

        bool Init() override;

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;

        template <typename T>
        void RegisterSample()
        {
            samples.emplace_back([this]() -> RHISampleBase* {
                auto *sample =  new T();
                sample->SetAPI(api);
                sample->OnStart();
                return sample;
            });
        }

    private:
        bool StartSample();
        void NextSample();
        void PrevSample();

        void RegisterPath();

        void OnKeyUp(KeyButtonType) override;
        std::vector<std::function<RHISampleBase*()>> samples;
        uint32_t currentIndex = 0;
        std::unique_ptr<RHISampleBase> currentSample;
        API api = API::DEFAULT;
    };

}
