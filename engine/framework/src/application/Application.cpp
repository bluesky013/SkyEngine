//
// Created by Zach Lee on 2021/11/11.
//

#include <framework/application/Application.h>

#include <core/profile/Profiler.h>

// std
#include <chrono>

#include <core/environment/Environment.h>
#include <core/logger/Logger.h>
#include <core/file/FileIO.h>

#include <framework/asset/AssetManager.h>
//#include <framework/database/DBManager.h>
#include <framework/platform/PlatformBase.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/World.h>

static const char *TAG = "Application";

namespace sky {

    Application::Application() : env(nullptr)
    {
    }

    Application::~Application()
    {
        if (moduleManager) {
            moduleManager->UnLoadModules();
        }

        Interface<ISystemNotify>::Get()->UnRegister();

        AssetManager::Destroy();

        sky::Platform* platform = sky::Platform::Get();
        platform->Shutdown();
    }

    void Application::SaveArgs(int argc, char **argv)
    {
        for (int i = 0; i < argc; ++i) {
            arguments.values.emplace_back(argv[i]);
            arguments.args.emplace_back(arguments.values.back().c_str());
        }
    }

    bool Application::Init(int argc, char **argv)
    {
        LOG_I(TAG, "Application Init Start...");
        env = Environment::Get();
        if (env == nullptr) {
            LOG_E(TAG, "Get Environment Failed");
            return false;
        }

        // save args
        SaveArgs(argc, argv);

        // module manager
        moduleManager = std::make_unique<ModuleManager>();

        // process args
        ParseStartArgs();

        // load configs
        LoadConfigs();

        Interface<ISystemNotify>::Get()->Register(*this);

        auto *context = SerializationContext::Get();
        World::Reflect(context);

        PreInit();

        // load dynamic modules
        if (moduleManager) {
            LOG_I(TAG, "Load Engine Module...");
            moduleManager->TopoSort();
            moduleManager->LoadModules(arguments);
            moduleManager->StartModules();
        }

        PostInit();
        LOG_I(TAG, "Load Engine Module Success");
        return true;
    }

    void Application::SetExit()
    {
        exit = true;
    }

    void Application::Loop()
    {
        SKY_FRAME_MARK("Game");
        SKY_PROFILE_NAME("App Loop")

        // pool events
        Platform::Get()->PoolEvent(exit);

        {
            SKY_PROFILE_NAME("PreTick")
            PreTick();
        }


        uint64_t        frequency      = Platform::Get()->GetPerformanceFrequency();
        uint64_t        currentCounter = Platform::Get()->GetPerformanceCounter();
        static uint64_t current        = 0;
        float           delta = current > 0 ? static_cast<float>(static_cast<float>(currentCounter - current) / static_cast<double>(frequency)) : 1.0f / 60.0f;
        current               = currentCounter;

        if (tickFn) {
            SKY_PROFILE_NAME("World Tick")
            tickFn(delta);
        }

        if (moduleManager) {
            SKY_PROFILE_NAME("Module Tick")
            moduleManager->Tick(delta);
        }
    }

    void Application::Mainloop()
    {
        while (!exit) {
            Loop();
        }
    }

} // namespace sky
