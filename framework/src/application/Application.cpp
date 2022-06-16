//
// Created by Zach Lee on 2021/11/11.
//

#include <framework/application/Application.h>
#include <core/environment/Environment.h>
#include <core/logger/Logger.h>
#include "PlatformImpl.h"
#include <chrono>

static const char* TAG = "Application";

namespace sky {
    
    using ModuleStart = IModule*(*)(Environment*);
    using ModuleStop = void(*)();

    ApplicationImpl* ApplicationImpl::Create()
    {
        return PlatformImpl::Get()->CreateApplication();
    }

    Application::Application() : impl(nullptr), env(nullptr)
    {
    }

    Application::~Application()
    {

    }

    bool Application::Init(StartInfo& start)
    {
        LOG_I(TAG, "Application Init Start...");
        settings.Swap(start.setting);

        impl = ApplicationImpl::Create();
        if (impl == nullptr) {
            LOG_E(TAG, "Init App Failed");
            return false;
        }

        env = Environment::Get();
        if (env == nullptr) {
            LOG_E(TAG, "Get Environment Failed");
            return false;
        }

        Interface<ISystemNotify>::Get()->Register(*this);

        LOG_I(TAG, "Load Engine Module...");
        LoadDynamicModules(start);

        if (start.createWindow) {
            nativeWindow.reset(NativeWindow::Create(NativeWindow::Descriptor{start.windowWidth, start.windowHeight, start.appName, start.appName}));
        }

        LOG_I(TAG, "Load Engine Module Success");
        return true;
    }

    void Application::LoadDynamicModules(const StartInfo& startInfo)
    {
        for (auto& module : startInfo.modules) {
            auto dynModule = std::make_unique<DynamicModule>(module);
            LOG_I(TAG, "Load Module : %s", module.c_str());
            if (dynModule->Load()) {
                auto startFn = dynModule->GetAddress<ModuleStart>("StartModule");
                if (startFn == nullptr) {
                    LOG_E(TAG, "Load Module : %s failed", module.c_str());
                    continue;
                }
                auto module = startFn(Environment::Get());
                if (module == nullptr) {
                    continue;
                }
                module->Init();
                modules.emplace_back(std::unique_ptr<IModule>(module));
                dynLibs.emplace_back(std::move(dynModule));
            }
            LOG_I(TAG, "Load Module : %s success", module.c_str());
        }

        for (auto& module : modules) {
            module->Start();
        }
    }

    void Application::UnloadDynamicModules()
    {
        for (auto& module : modules) {
            module->Stop();
        }

        for (auto& lib : dynLibs) {
            auto stopFn = lib->GetAddress<ModuleStop>("StopModule");
            if (stopFn != nullptr) {
                stopFn();
            }
        }
    }

    ApplicationImpl* Application::GetImpl() const
    {
        return impl;
    }

    void Application::SetExit()
    {
        if (impl != nullptr) {
            impl->SetExit();
        }
    }

    const SettingRegistry& Application::GetSettings() const
    {
        return settings;
    }

    const NativeWindow* Application::GetViewport() const
    {
        return nativeWindow.get();
    }

    void Application::Shutdown()
    {
        UnloadDynamicModules();

        Interface<ISystemNotify>::Get()->UnRegister();
    }

    void Application::Mainloop()
    {
        while (!impl->IsExit()) {
            impl->PumpMessages();

            static auto timePoint = std::chrono::high_resolution_clock::now();
            auto current = std::chrono::high_resolution_clock::now();
            auto delta = std::chrono::duration<float>(current - timePoint).count();
            timePoint = current;

            for (auto& module : modules) {
                module->Tick(delta);
            }
        }
    }


}
