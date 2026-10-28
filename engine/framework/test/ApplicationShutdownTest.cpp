#include <framework/application/Application.h>
#include <framework/asset/AssetExecutor.h>
#include <framework/platform/PlatformBase.h>
#include <framework/platform/Process.h>
#include <framework/world/WorldAsset.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

    enum class StartupStep { ModuleInit, ModuleStart, StartupData, WorldAttached, PostInit, ApplicationShutdown, WorldDetached, ModuleShutdown };

    class StartupWorldProbe final : public sky::IWorldSubSystem {
    public:
        explicit StartupWorldProbe(std::vector<StartupStep> &events) : events(events)
        {
        }
        void OnAttachToWorld(sky::World &) override
        {
            events.push_back(StartupStep::WorldAttached);
        }
        void OnDetachFromWorld(sky::World &) override
        {
            events.push_back(StartupStep::WorldDetached);
        }

    private:
        std::vector<StartupStep> &events;
    };

    class StartupModuleProbe final : public sky::IModule {
    public:
        explicit StartupModuleProbe(std::vector<StartupStep> &events) : events(events)
        {
        }
        bool Init(const sky::StartArguments &) override
        {
            events.push_back(StartupStep::ModuleInit);
            return true;
        }
        void Start() override
        {
            events.push_back(StartupStep::ModuleStart);
            sky::WorldSubSystemRegistration registration;
            registration.factory = [this](sky::World &, const sky::Any &) { return std::make_unique<StartupWorldProbe>(events); };
            sky::WorldSubSystemRegistry::Get().Register(sky::Name("StartupWorldProbe"), std::move(registration));
        }
        void Shutdown() override
        {
            events.push_back(StartupStep::ModuleShutdown);
            sky::WorldSubSystemRegistry::Get().Unregister(sky::Name("StartupWorldProbe"));
        }

    private:
        std::vector<StartupStep> &events;
    };

    class StartupApplicationProbe final : public sky::Application {
    public:
        StartupApplicationProbe(std::vector<StartupStep> &events, bool failStartup) : events(events), failStartup(failStartup)
        {
        }

    private:
        bool LoadConfigs() override
        {
            moduleManager->AddStaticModule("StartupModuleProbe", std::make_unique<StartupModuleProbe>(events));
            moduleManager->RegisterModule({"StartupModuleProbe", {}});
            return true;
        }
        bool InitStartupData() override
        {
            events.push_back(StartupStep::StartupData);
            sky::WorldAssetData data;
            data.subSystems.push_back({"StartupWorldProbe", true, {}, {}});
            std::string error;
            return sky::InstantiateWorldAsset(data, world, error) && !failStartup;
        }
        void PostInit() override
        {
            events.push_back(StartupStep::PostInit);
        }
        void OnShutdown() override
        {
            events.push_back(StartupStep::ApplicationShutdown);
            world = nullptr;
        }

        std::vector<StartupStep> &events;
        bool                      failStartup;
        sky::WorldPtr             world;
    };

    void CheckStartupProcess(const char *mode)
    {
#if SKY_PLATFORM_ANDROID || SKY_PLATFORM_IOS
        GTEST_SKIP() << "native subprocess validation requires a desktop host";
#else
        if (!sky::Platform::Get()->GetImpl()) {
            ASSERT_TRUE(sky::Platform::Get()->Init({}));
        }
        const auto executable = std::filesystem::path(sky::Platform::Get()->GetBundlePath()) /
    #if SKY_PLATFORM_WINDOWS
                                "FrameworkTest.exe";
    #else
                                "FrameworkTest";
    #endif
        sky::ProcessDesc desc;
        desc.args    = {executable.string(), "--application-startup-helper", mode};
        auto process = sky::CreateProcess();
        ASSERT_NE(process, nullptr);
        ASSERT_TRUE(process->Start(desc)) << executable.string();
        int        exitCode = -1;
        const bool exited   = process->WaitFor(10000, exitCode);
        if (!exited) {
            process->Kill();
            process->WaitFor(5000, exitCode);
        }
        ASSERT_TRUE(exited) << mode;
        EXPECT_EQ(exitCode, 0) << mode;
#endif
    }

    class ShutdownProbe : public sky::IModule {
    public:
        ShutdownProbe(std::atomic_bool &done, std::atomic_bool &early) : workerDone(done), stoppedEarly(early)
        {
        }

        void Shutdown() override
        {
            stoppedEarly.store(!workerDone.load());
        }

    private:
        std::atomic_bool &workerDone;
        std::atomic_bool &stoppedEarly;
    };

    class TeardownApplication : public sky::Application {
    public:
        TeardownApplication(std::atomic_bool &done, std::atomic_bool &early)
        {
            moduleManager = std::make_unique<sky::ModuleManager>();
            moduleManager->AddStaticModule("ShutdownProbe", std::make_unique<ShutdownProbe>(done, early));
            moduleManager->RegisterModule({"ShutdownProbe", {}});
            moduleManager->TopoSort();
            moduleManager->LoadModules({});
        }
    };

    class OrderedShutdownApplication : public sky::Application {
    public:
        explicit OrderedShutdownApplication(std::vector<int> &events) : events(events)
        {
            class Probe final : public sky::IModule {
            public:
                explicit Probe(std::vector<int> &events) : events(events)
                {
                }
                void Shutdown() override
                {
                    events.push_back(2);
                }

            private:
                std::vector<int> &events;
            };
            moduleManager = std::make_unique<sky::ModuleManager>();
            moduleManager->AddStaticModule("OrderedShutdownProbe", std::make_unique<Probe>(events));
            moduleManager->TopoSort();
            moduleManager->LoadModules({});
        }

    private:
        void OnShutdown() override
        {
            events.push_back(1);
            Shutdown();
        }
        std::vector<int> &events;
    };

} // namespace

std::optional<int> RunApplicationStartupProcessHelper(int argc, char **argv)
{
    if (argc != 3 || std::string(argv[1]) != "--application-startup-helper") {
        return std::nullopt;
    }
    const std::string mode = argv[2];
    if (mode != "success" && mode != "failure") {
        return 2;
    }
    const bool               failStartup = mode == "failure";
    std::vector<StartupStep> events;
    std::vector<StartupStep> expected{StartupStep::ModuleInit, StartupStep::ModuleStart, StartupStep::StartupData, StartupStep::WorldAttached};
    {
        StartupApplicationProbe application(events, failStartup);
        if (application.Init(argc, argv) == failStartup) {
            return 3;
        }
        if (failStartup) {
            expected.insert(expected.end(), {StartupStep::ApplicationShutdown, StartupStep::WorldDetached, StartupStep::ModuleShutdown});
        } else {
            expected.push_back(StartupStep::PostInit);
        }
        if (events != expected) {
            return 4;
        }
        application.Shutdown();
        application.Shutdown();
        if (!failStartup) {
            expected.insert(expected.end(), {StartupStep::ApplicationShutdown, StartupStep::WorldDetached, StartupStep::ModuleShutdown});
        }
        if (events != expected || sky::WorldSubSystemRegistry::Get().IsRegistered(sky::Name("StartupWorldProbe"))) {
            return 5;
        }
    }
    return events == expected ? 0 : 6;
}

TEST(ApplicationShutdownTest, RuntimeStartupDataSeesServicesRegisteredByModuleStart)
{
    CheckStartupProcess("success");
}

TEST(ApplicationShutdownTest, StartupDataFailureReleasesWorldBeforeModulesAndSkipsPostInit)
{
    CheckStartupProcess("failure");
}

TEST(ApplicationShutdownTest, OwnedStartupStateStopsOnceBeforeModulesEvenOnReentry)
{
    std::vector<int>           events;
    OrderedShutdownApplication application(events);
    application.Shutdown();
    application.Shutdown();
    EXPECT_EQ(events, (std::vector<int>{1, 2}));
}

TEST(ApplicationShutdownTest, AssetWorkCompletesBeforeModuleShutdown)
{
    std::atomic_bool   workerDone{false};
    std::atomic_bool   stoppedEarly{true};
    auto               application = std::make_unique<TeardownApplication>(workerDone, stoppedEarly);
    std::promise<void> entered;
    std::promise<void> release;
    auto               ready = entered.get_future();
    auto               gate  = release.get_future().share();
    auto               work  = sky::AssetExecutor::Get()->DependentAsync([&entered, &workerDone, gate]() {
        entered.set_value();
        gate.wait();
        workerDone.store(true);
    });
    ready.wait();
    std::thread unblock([&release]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        release.set_value();
    });
    application.reset(); // Native platform teardown remains on the main thread.
    unblock.join();
    work.second.get();
    EXPECT_FALSE(stoppedEarly.load());
}
