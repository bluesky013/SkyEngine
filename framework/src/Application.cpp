//
// Created by Zach Lee on 2021/11/11.
//

#include <framework/Application.h>
#include <framework/environment/Environment.h>
#include <core/logger/Logger.h>
#include "PlatformImpl.h"
#include <chrono>

static const char* TAG = "Application";

namespace sky {

    using EngineLoad = IEngine*(*)(Environment*);
    using EngineShutdown = void(*)(IEngine*);
    using ModuleStart = void(*)(Application&, Environment*);

    ApplicationImpl* ApplicationImpl::Create()
    {
        return PlatformImpl::Get()->CreateApplication();
    }

    Application::Application() : impl(nullptr), engineInstance(nullptr), env(nullptr)
    {
    }

    Application::~Application()
    {

    }

    bool Application::Init(const StartInfo& start)
    {
        LOG_I(TAG, "Application Init Start...");
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

        LOG_I(TAG, "Load Engine Module...");
        engineModule = std::make_unique<DynamicModule>("SkyEngineModule");
        if (engineModule->Load()) {
            auto createFn = engineModule->GetAddress<EngineLoad>("StartEngine");
            if (createFn != nullptr) {
                engineInstance = createFn(env);
                engineInstance->Init(start);
            }
        }
        LOG_I(TAG, "Load Engine Module Success");


        for (auto& module : start.modules) {
            auto dynModule = std::make_unique<DynamicModule>(module);
            LOG_I(TAG, "Load Module : %s", module.c_str());
            if (dynModule->Load()) {
                auto startFn = dynModule->GetAddress<ModuleStart>("StartModule");
                if (startFn == nullptr) {
                    LOG_E(TAG, "Load Module : %s failed", module.c_str());
                    continue;
                }
                startFn(*this, env);
                modules.emplace_back(dynModule.release());
            }
            LOG_I(TAG, "Load Module : %s success", module.c_str());
        }
        LOG_I(TAG, "Application Init Success");
        return true;
    }

    NativeWindow* Application::CreateNativeWindow(const NativeWindow::Descriptor& des)
    {
        auto window = NativeWindow::Create(des);
        window->SetEventHandler(*engineInstance->GetEventHandler());
        window->SetApplication(*this);
        return window;
    }

    IEngine* Application::GetEngine() const
    {
        return engineInstance;
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

    void Application::Shutdown()
    {
        if (engineModule->IsLoaded()) {
            auto destroyFn = engineModule->GetAddress<EngineShutdown>("ShutdownEngine");
            if (destroyFn != nullptr) {
                engineInstance->DeInit();
                destroyFn(engineInstance);
            }
        }
    }

    void Application::Mainloop()
    {
        while (!impl->IsExit()) {
            impl->PumpMessages();

            static auto timePoint = std::chrono::high_resolution_clock::now();
            auto current = std::chrono::high_resolution_clock::now();
            auto delta = std::chrono::duration<float>(current - timePoint).count();
            timePoint = current;
            if (engineInstance != nullptr) {
                engineInstance->Tick(delta);
            }
        }
    }


}
