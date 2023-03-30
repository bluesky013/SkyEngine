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
#include "VulkanVariableRateShading.h"
#include "VulkanMultiViewSample.h"

namespace sky {

    bool VulkanSample::Init()
    {
        RegisterSample<VulkanTriangleSample>();
        RegisterSample<VulkanMemoryAliasing>();
        RegisterSample<VulkanMultiViewSample>();
//        RegisterSample<VulkanAsyncUploadSample>();
#ifdef WIN32
        RegisterSample<VulkanDescriptorSample>();
//        RegisterSample<VulkanTerrainVTSample>();
        RegisterSample<VulkanBindlessSample>();
        RegisterSample<VulkanSparseImageSample>();
        RegisterSample<VulkanVariableRateShading>();
#endif

        auto nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
        Event<IWindowEvent>::Connect(nativeWindow->GetNativeHandle(), this);

        return true;
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

    bool VulkanSample::StartSample()
    {
        if (currentSample) {
            currentSample->OnStop();
        }
        currentSample.reset(samples[currentIndex]());
        return currentSample->CheckFeature();
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
        if (button == KeyButton::KEY_F2) {
            NextSample();
            while (!StartSample()) {
                NextSample();
            }
        } else if (button == KeyButton::KEY_F3) {
            PrevSample();
            StartSample();
        }
    }
}
REGISTER_MODULE(sky::VulkanSample)
