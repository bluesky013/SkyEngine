//
// Created by Zach Lee on 2022/11/5.
//

#include "RHISample.h"
#include "RHISampleBase.h"

namespace sky::rhi {

    void RHISample::Init()
    {
        RegisterSample<RHISampleBase>();

        auto nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
        Event<IWindowEvent>::Connect(nativeWindow->GetNativeHandle(), this);
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
