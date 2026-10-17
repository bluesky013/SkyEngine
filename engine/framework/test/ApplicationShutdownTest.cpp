#include <framework/application/Application.h>
#include <framework/asset/AssetExecutor.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>

namespace {

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

} // namespace

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
