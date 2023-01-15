//
// Created by Zach Lee on 2022/11/5.
//

#include <VulkanSample.h>
#include "VulkanTriangleSample.h"
#include "VulkanMemoryAliasing.h"
#include "VulkanDescriptorSample.h"
#include "VulkanBindlessSample.h"
#include "VulkanAsyncUploadSample.h"
#include "VulkanSparseImageSample.h"
#include "VulkanTerrainVTSample.h"

namespace sky {

    void VulkanSample::Init()
    {
        RegisterSample<VulkanTerrainVTSample>();
        RegisterSample<VulkanTriangleSample>();
        RegisterSample<VulkanMemoryAliasing>();
        RegisterSample<VulkanDescriptorSample>();
        RegisterSample<VulkanBindlessSample>();
        RegisterSample<VulkanAsyncUploadSample>();
        RegisterSample<VulkanSparseImageSample>();

        auto nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
        Event<IWindowEvent>::Connect(nativeWindow->GetNativeHandle(), this);
    }

    void VulkanSample::Start()
    {
        StartSample();
    }

    void VulkanSample::Stop()
    {
        if (currentSample) {
            currentSample->OnStop();
        }
        currentSample = nullptr;
        Event<IWindowEvent>::DisConnect(this);
    }

    void VulkanSample::Tick(float delta)
    {
        if (currentSample) {
            currentSample->OnTick(delta);
        }
    }

    void VulkanSample::StartSample()
    {
        if (currentSample) {
            currentSample->OnStop();
        }
        currentSample.reset(samples[currentIndex]());
    }

    void VulkanSample::NextSample()
    {
        currentIndex = (currentIndex + 1) % samples.size();
    }

    void VulkanSample::PrevSample()
    {
        currentIndex = (currentIndex + static_cast<uint32_t>(samples.size()) - 1) % samples.size();
    }

    void VulkanSample::OnKeyUp(KeyButtonType button)
    {
        if (button == KeyButton::KEY_PAGEUP) {
            NextSample();
            StartSample();
        } else if (button == KeyButton::KEY_PAGEDOWN) {
            PrevSample();
            StartSample();
        }
    }
}
REGISTER_MODULE(sky::VulkanSample)
