//
// Created by Zach Lee on 2022/11/5.
//

#include "DXSample.h"
#include "DXSampleBase.h"

namespace sky {

    void DX12Sample::Init()
    {
        RegisterSample<DX12SampleBase>();

        auto nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
        Event<IWindowEvent>::Connect(nativeWindow->GetNativeHandle(), this);
    }

    void DX12Sample::Start()
    {
        StartSample();
    }

    void DX12Sample::Stop()
    {
        if (currentSample) {
            currentSample->OnStop();
        }
        currentSample = nullptr;
        Event<IWindowEvent>::DisConnect(this);
    }

    void DX12Sample::Tick(float delta)
    {
        if (currentSample) {
            currentSample->OnTick(delta);
        }
    }

    void DX12Sample::StartSample()
    {
        if (currentSample) {
            currentSample->OnStop();
        }
        currentSample.reset(samples[currentIndex]());
    }

    void DX12Sample::NextSample()
    {
        currentIndex = (currentIndex + 1) % samples.size();
    }

    void DX12Sample::PrevSample()
    {
        currentIndex = (currentIndex + static_cast<uint32_t>(samples.size()) - 1) % samples.size();
    }

    void DX12Sample::OnKeyUp(KeyButtonType button)
    {
        if (button == KeyButton::KEY_RIGHT) {
            NextSample();
        } else if (button == KeyButton::KEY_LEFT) {
            PrevSample();
        }

        StartSample();
    }

}
REGISTER_MODULE(sky::DX12Sample)