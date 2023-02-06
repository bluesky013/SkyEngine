//
// Created by Zach Lee on 2022/11/5.
//

#include "RHISample.h"
#include "RHISampleBase.h"
#include <framework/application/SettingRegistry.h>

namespace sky::rhi {

    static const bool API_CHECK[] = {
        false,
        true,
        false,
        false,
        true
    };

    bool RHISample::Init()
    {
        RegisterSample<RHISampleBase>();

        auto systemApi = Interface<ISystemNotify>::Get()->GetApi();
        auto &settings = systemApi->GetSettings();
        auto rhi = settings.VisitString("rhi");
        if (rhi == "gles") {
            api = API::GLES;
        } else if (rhi == "vulkan") {
            api = API::VULKAN;
        } else if (rhi == "dx12") {
            api = API::DX12;
        } else if (rhi == "metal") {
            api = API::METAL;
        }
        if (API_CHECK[static_cast<uint32_t>(api)]) {
            auto nativeWindow = systemApi->GetViewport();
            Event<IWindowEvent>::Connect(nativeWindow->GetNativeHandle(), this);
            return true;
        }
        return false;
    }

    void RHISample::Start()
    {
        StartSample();
    }

    void RHISample::Stop()
    {
        if (currentSample) {
            currentSample->OnStop();
        }
        currentSample = nullptr;
        Event<IWindowEvent>::DisConnect(this);
    }

    void RHISample::Tick(float delta)
    {
        if (currentSample) {
            currentSample->OnTick(delta);
        }
    }

    void RHISample::StartSample()
    {
        if (currentSample) {
            currentSample->OnStop();
        }
        currentSample.reset(samples[currentIndex]());
    }

    void RHISample::NextSample()
    {
        currentIndex = (currentIndex + 1) % samples.size();
    }

    void RHISample::PrevSample()
    {
        currentIndex = (currentIndex + static_cast<uint32_t>(samples.size()) - 1) % samples.size();
    }

    void RHISample::OnKeyUp(KeyButtonType button)
    {
        if (button == KeyButton::KEY_RIGHT) {
            NextSample();
        } else if (button == KeyButton::KEY_LEFT) {
            PrevSample();
        }

        StartSample();
    }

}
REGISTER_MODULE(sky::rhi::RHISample)
