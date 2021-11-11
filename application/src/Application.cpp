//
// Created by Zach Lee on 2021/11/11.
//

#include <application/Application.h>

namespace sky {

    using EngineLoad = void*(*)();
    using EngineShutdown = void(*)(void*);

    Application::Application() : impl(nullptr), window(nullptr), engineInstance(nullptr)
    {
    }

    Application::~Application()
    {

    }

    bool Application::Init()
    {
        impl = Impl::Create();
        if (impl == nullptr) {
            return false;
        }

        NativeWindow::Descriptor des = {};
        des.titleName = "SkyEngine";
        des.className = "SkyEngine";

        window = NativeWindow::Create(des);
        if (window == nullptr) {
            return false;
        }

        module = std::make_unique<DynamicModule>("SkyEngine");
        if (module->Load()) {
            auto createFn = module->GetAddress<EngineLoad>("StartEngine");
            if (createFn != nullptr) {
                engineInstance = createFn();
            }
        }
        return true;
    }

    void Application::Shutdown()
    {
        if (module->IsLoaded()) {
            auto destroyFn = module->GetAddress<EngineShutdown>("ShutdownEngine");
            if (destroyFn != nullptr) {
                destroyFn(engineInstance);
            }
        }
    }

    void Application::Mainloop()
    {
        while (!impl->IsExit()) {
            impl->PumpMessages();
        }
    }


}
