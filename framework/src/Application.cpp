//
// Created by Zach Lee on 2021/11/11.
//

#include <framework/Application.h>
#include <framework/environment/Environment.h>
#include "PlatformImpl.h"
#include <chrono>

namespace sky {

    using EngineLoad = IEngine*(*)(Environment*);
    using EngineShutdown = void(*)(IEngine*);
    using ModuleStart = void(*)(Application&, Environment*);

    Application::Impl* Application::Impl::Create()
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
        impl = Impl::Create();
        if (impl == nullptr) {
            return false;
        }

        env = Environment::Get();
        if (env == nullptr) {
            return false;
        }

        engine = std::make_unique<DynamicModule>("SkyEngineModule");
        if (engine->Load()) {
            auto createFn = engine->GetAddress<EngineLoad>("StartEngine");
            if (createFn != nullptr) {
                engineInstance = createFn(env);
                engineInstance->Init(start);
            }
        }

        for (auto& module : start.modules) {
            auto dynModule = std::make_unique<DynamicModule>(module);
            if (dynModule->Load()) {
                auto startFn = dynModule->GetAddress<ModuleStart>("StartModule");
                if (startFn == nullptr) {
                    continue;
                }
                startFn(*this, env);
                modules.emplace_back(dynModule.release());
            }
        }
        return true;
    }

    NativeWindow* Application::CreateNativeWindow(const NativeWindow::Descriptor& des)
    {
        return NativeWindow::Create(des);
    }

    IEngine* Application::GetEngine() const
    {
        return engineInstance;
    }

    void Application::SetExit()
    {
        if (impl != nullptr) {
            impl->SetExit();
        }
    }

    void Application::Shutdown()
    {
        if (engine->IsLoaded()) {
            auto destroyFn = engine->GetAddress<EngineShutdown>("ShutdownEngine");
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
        }

        static auto timePoint = std::chrono::high_resolution_clock::now();
        auto current = std::chrono::high_resolution_clock::now();
        auto delta = std::chrono::duration<float>(current - timePoint).count();
        timePoint = current;
        if (engineInstance != nullptr) {
            engineInstance->Tick(delta);
        }
    }


}
