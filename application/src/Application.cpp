//
// Created by Zach Lee on 2021/11/11.
//

#include <application/Application.h>
#include <chrono>

namespace sky {

    using EngineLoad = IEngine*(*)();
    using EngineShutdown = void(*)(IEngine*);
    using ModuleStart = void(*)(Application&);

    Application::Application() : impl(nullptr), engineInstance(nullptr)
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

        engine = std::make_unique<DynamicModule>("SkyEngineModule");
        if (engine->Load()) {
            auto createFn = engine->GetAddress<EngineLoad>("StartEngine");
            if (createFn != nullptr) {
                engineInstance = createFn();
                engineInstance->Init(start);
            }
        }

        for (auto& module : start.modules) {
            auto dynModule = std::make_unique<DynamicModule>(module);
            if (dynModule->Load()) {
                auto startFn = dynModule->GetAddress<ModuleStart>("StartModule");
                if (startFn != nullptr) {
                    startFn(*this);
                }
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
