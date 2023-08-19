//
// Created by Zach Lee on 2022/11/5.
//

#include "RHISample.h"
#include "RHISampleBase.h"
#include "RHIPassSample.h"
#include "RHISubPassSample.h"
#include "RHIDrawIndirectSample.h"
#include "RHISubpassMSAA.h"
#include <framework/application/SettingRegistry.h>
#include <framework/asset/AssetManager.h>
#include <framework/platform/PlatformBase.h>
#include <EngineRoot.h>
#include <RHISample/ProjectRoot.h>
#include <filesystem>

namespace sky::rhi {

    static const bool API_CHECK[] = {
        true,
        true,
        true,
        true,
        true
    };

    bool RHISample::Init()
    {
        RegisterPath();

        RegisterSample<RHISampleBase>();
        RegisterSample<RHISubPassMSAA>();
        RegisterSample<RHIDrawIndirectSample>();
        RegisterSample<RHIPassSample>();
        RegisterSample<RHISubPassSample>();

        auto *systemApi = Interface<ISystemNotify>::Get()->GetApi();
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
        } else {
            api = API::VULKAN;
        }
        if (API_CHECK[static_cast<uint32_t>(api)]) {
            const auto *nativeWindow = systemApi->GetViewport();
            Event<IWindowEvent>::Connect(nativeWindow, this);
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

    bool RHISample::StartSample()
    {
        if (currentSample) {
            currentSample->OnStop();
        }
        currentSample.reset(samples[currentIndex]());
        return currentSample->CheckFeature();
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

    void RHISample::RegisterPath()
    {
        AssetManager::Get()->RegisterSearchPath(ENGINE_ROOT + "/assets");
        AssetManager::Get()->RegisterSearchPath(PROJECT_ROOT + "/assets");

        auto path = Platform::Get()->GetInternalPath();
        std::filesystem::create_directories(path + "/shaders/RHISample");
    }
}
REGISTER_MODULE(sky::rhi::RHISample)
