//
// Created by Zach Lee on 2021/11/11.
//

#include <framework/application/Application.h>
#include <core/environment/Environment.h>
#include <core/logger/Logger.h>
#include <SDL2/SDL.h>
#include <chrono>

static const char* TAG = "Application";

namespace sky {
    
    using ModuleStart = IModule*(*)(Environment*);
    using ModuleStop = void(*)();

    Application::Application() : env(nullptr)
    {
    }

    Application::~Application()
    {

    }

    bool Application::Init(StartInfo& start)
    {
        LOG_I(TAG, "Application Init Start...");

        if(SDL_Init(SDL_INIT_EVERYTHING) < 0) {
            LOG_E(TAG, "SDL could not be initialized! Error: %s", SDL_GetError());
            return false;
        }

        env = Environment::Get();
        if (env == nullptr) {
            LOG_E(TAG, "Get Environment Failed");
            return false;
        }

        if (start.createWindow) {
            nativeWindow.reset(NativeWindow::Create(NativeWindow::Descriptor{start.windowWidth, start.windowHeight, start.appName, start.appName}));
        }

        Interface<ISystemNotify>::Get()->Register(*this);

        LOG_I(TAG, "Load Engine Module...");
        LoadDynamicModules(start);

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

    void Application::SetExit()
    {
        exit = true;
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

        SDL_Quit();
    }

    void Application::Mainloop()
    {
        while (!exit) {
            nativeWindow->PollEvent(exit);

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
