//
// Created by Zach Lee on 2021/11/11.
//

#include <framework/application/Application.h>

#include <core/profile/Profiler.h>

#include <core/environment/Environment.h>
#include <core/logger/Logger.h>

#include <framework/asset/AssetExecutor.h>
#include <framework/asset/AssetManager.h>
// #include <framework/database/DBManager.h>
#include <core/thread/ThreadDispatcher.h>
#include <framework/platform/PlatformBase.h>
#include <framework/serialization/SerializationContext.h>

#if defined(SKY_MODULE_STATIC)
namespace sky {
    class ModuleManager;
}
// Provided by the generated RegisterStaticModules.cpp (static module packaging).
extern "C" void SkyEngineRegisterStaticModules(sky::ModuleManager &manager);
#endif

static const char *TAG = "Application";

namespace sky {

    namespace {
        void DrainApplicationCallbacks()
        {
            auto *dispatcher = ThreadDispatcher::Get();
            do {
                AssetExecutor::Get()->WaitForAll();
            } while (dispatcher->Dispatch(ThreadRole::Main) != 0);
        }
    } // namespace

    Application::Application() : env(nullptr)
    {
    }

    Application::~Application()
    {
        Shutdown();

        Interface<ISystemNotify>::Get()->UnRegister();

        tickFn = {};
        DrainApplicationCallbacks();
        moduleManager.reset();
        DrainApplicationCallbacks();

        AssetManager::Destroy();

        sky::Platform *platform = sky::Platform::Get();
        platform->Shutdown();
        const auto released = ThreadDispatcher::Get()->ReleaseRole(ThreadRole::Main);
        if (released != ThreadDispatcher::ReleaseResult::RELEASED) {
            LOG_E(TAG, "Application main dispatcher role release failed: %u", static_cast<unsigned>(released));
        }
    }

    void Application::Shutdown()
    {
        if (shutdown) {
            return;
        }
        shutdown = true;
        Platform::Get()->StopFrameLoop();
        if (Platform::Get()->GetImpl() != nullptr) {
            Platform::Get()->SetFrameCallback({});
        }
        OnShutdown();
        // Workers may still use module code and registries while completing queued work.
        DrainApplicationCallbacks();
        if (moduleManager) {
            moduleManager->UnLoadModules();
        }
        DrainApplicationCallbacks();
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
        if (!LoadConfigs()) {
            LOG_E(TAG, "Load Configs Failed");
            return false;
        }

        Interface<ISystemNotify>::Get()->Register(*this);

        SerializationContext::Get();

        if (!PreInit()) {
            LOG_E(TAG, "Application PreInit Failed");
            return false;
        }

        // load modules (static bundling registers compiled-in modules first)
        if (moduleManager) {
            LOG_I(TAG, "Load Engine Module...");
#if defined(SKY_MODULE_STATIC)
            SkyEngineRegisterStaticModules(*moduleManager);
#endif
            moduleManager->TopoSort();
            if (!moduleManager->LoadModules(arguments)) {
                LOG_E(TAG, "Required module initialization failed");
                Shutdown();
                return false;
            }
            moduleManager->StartModules();
            if (!InitStartupData()) {
                LOG_E(TAG, "Application startup data failed to load");
                Shutdown();
                return false;
            }
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

        // A quit request (e.g. the main window closed) must stop the frame
        // before rendering: the window may already be destroyed.
        if (exit) {
            Platform::Get()->StopFrameLoop();
            return;
        }

        if (!Platform::Get()->IsFrameActive()) {
            frameClock.Reset();
            return;
        }

        // Run cross-thread callbacks posted to the main role before this frame's tick work.
        ThreadDispatcher::Get()->Dispatch(ThreadRole::Main);

        {
            SKY_PROFILE_NAME("PreTick")
            PreTick();
        }

        const uint64_t frequency      = Platform::Get()->GetPerformanceFrequency();
        const uint64_t currentCounter = Platform::Get()->GetPerformanceCounter();
        const float    delta          = frameClock.Sample(currentCounter, frequency, Platform::Get()->GetFrameEpoch());

        Tick(delta);

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
        // Platforms that own the frame loop (iOS/CADisplayLink) set the external
        // frame loop flag; the host calls Loop() each frame instead.
        if (externalFrameLoop) {
            return;
        }
        while (!exit) {
            Loop();
        }
    }

} // namespace sky
