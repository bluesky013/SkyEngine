//
// Created by Zach Lee on 2021/11/11.
//

#include <chrono>
#include <core/environment/Environment.h>
#include <core/logger/Logger.h>
#include <framework/application/Application.h>
#include <framework/platform/PlatformBase.h>

static const char *TAG = "Application";

namespace sky {

    using ModuleStart = IModule *(*)(Environment *);
    using ModuleStop  = void (*)();

    Application::Application() : env(nullptr)
    {
    }

    Application::~Application()
    {
    }

    bool Application::Init(StartInfo &start)
    {
        LOG_I(TAG, "Application Init Start...");

        env = Environment::Get();
        if (env == nullptr) {
            LOG_E(TAG, "Get Environment Failed");
            return false;
        }

        // merge settings
        settings.Swap(start.setting);

        Interface<ISystemNotify>::Get()->Register(*this);

        LOG_I(TAG, "Load Engine Module...");
        LoadDynamicModules(start);

        LOG_I(TAG, "Load Engine Module Success");
        return true;
    }

    void Application::LoadDynamicModules(const StartInfo &startInfo)
    {
        for (auto &module : startInfo.modules) {
            auto dynModule = std::make_unique<DynamicModule>(module);
            LOG_I(TAG, "Load Module : %s", module.c_str());
            if (dynModule->Load()) {
                auto startFn = dynModule->GetAddress<ModuleStart>("StartModule");
                if (startFn == nullptr) {
                    LOG_E(TAG, "Load Module : %s failed", module.c_str());
                    continue;
                }
                auto mod = startFn(Environment::Get());
                if (mod == nullptr) {
                    continue;
                }
                if (!mod->Init()) {
                    continue;
                }
                modules.emplace_back(std::unique_ptr<IModule>(mod));
                dynLibs.emplace_back(std::move(dynModule));
            }
            LOG_I(TAG, "Load Module : %s success", module.c_str());
        }

        for (auto &module : modules) {
            module->Start();
        }
    }

    void Application::UnloadDynamicModules()
    {
        for (auto &module : modules) {
            module->Stop();
        }

        for (auto &lib : dynLibs) {
            auto stopFn = lib->GetAddress<ModuleStop>("StopModule");
            if (stopFn != nullptr) {
                stopFn();
            }
        }
    }

    void Application::SetExit()
    {
        exit = true;
    }

    SettingRegistry &Application::GetSettings()
    {
        return settings;
    }

    void Application::Shutdown()
    {
        UnloadDynamicModules();
        Interface<ISystemNotify>::Get()->UnRegister();
    }

    void Application::Loop()
    {
        PreTick();

        uint64_t        frequency      = PlatformBase::GetPlatform()->GetPerformanceFrequency();
        uint64_t        currentCounter = PlatformBase::GetPlatform()->GetPerformanceCounter();
        static uint64_t current        = 0;
        float           delta = current > 0 ? static_cast<float>((currentCounter - current) / static_cast<double>(frequency)) : 1.0f / 60.0f;
        current               = currentCounter;

        for (auto &module : modules) {
            module->Tick(delta);
        }

        if (tickFn) {
            tickFn(delta);
        }
    }

    void Application::Mainloop()
    {
        while (!exit) {
            Loop();
        }
    }

} // namespace sky
