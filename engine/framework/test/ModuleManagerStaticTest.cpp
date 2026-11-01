//
// ModuleManager: statically-registered (compiled-in) modules load without dlopen.
//

#include <framework/application/ModuleManager.h>
#include <framework/interface/IModule.h>

#include <framework/window/IWindowEvent.h>
#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace {

    class TestStaticModule : public sky::IModule {
    public:
        bool Init(const sky::StartArguments &) override
        {
            inited = true;
            return true;
        }
        void Start() override
        {
            started = true;
        }

        bool inited  = false;
        bool started = false;
    };

} // namespace

TEST(ModuleManagerStaticTest, StaticModuleLoadsWithoutDlopen)
{
    sky::ModuleManager manager;
    auto               module = std::make_unique<TestStaticModule>();
    TestStaticModule  *raw    = module.get();

    manager.AddStaticModule("TestStaticModule", std::move(module));
    manager.TopoSort();

    sky::StartArguments args;
    manager.LoadModules(args);
    manager.StartModules();

    EXPECT_TRUE(raw->inited);
    EXPECT_TRUE(raw->started);
}

TEST(ModuleManagerStaticTest, RuntimeBuildOptionsAreKnownAndUnknownOptionsStayDistinctFromDisabled)
{
    EXPECT_TRUE(sky::ModuleManager::GetBuildOption("SKY_BUILD_ANIMATION").has_value());
    EXPECT_TRUE(sky::ModuleManager::GetBuildOption("SKY_BUILD_PYTHON").has_value());
    EXPECT_FALSE(sky::ModuleManager::GetBuildOption("SKY_BUILD_UNKNOWN_FEATURE").has_value());
}

TEST(ModuleManagerStaticTest, DestructionStopsInitializedSessionsInDependencyOrder)
{
    std::vector<int> stopped;
    struct Session   final : sky::IModule {
        Session(sky::ModuleManager &manager, std::vector<int> &stopped, int id) : manager(manager), stopped(stopped), id(id)
        {
        }
        ~Session() override
        {
            stopped.push_back(-id);
        }
        void Shutdown() override
        {
            EXPECT_FALSE(manager.IsModuleInitialized(id == 1 ? "ResidentProviderSession" : "ResidentConsumerSession"));
            EXPECT_FALSE(manager.LoadModules({}));
            EXPECT_FALSE(manager.StartModules());
            EXPECT_FALSE(manager.Tick(0.f));
            EXPECT_TRUE(manager.UnLoadModules());
            EXPECT_FALSE(manager.RegisterModule({"ShutdownRegistration", {}}));
            EXPECT_FALSE(manager.AddStaticModule("ShutdownStaticRegistration", std::make_unique<sky::IModule>()));
            EXPECT_FALSE(manager.TopoSort());
            stopped.push_back(id);
        }
        sky::ModuleManager &manager;
        std::vector<int>   &stopped;
        int                 id;
    };
    {
        sky::ModuleManager manager;
        manager.AddStaticModule("ResidentProviderSession", std::make_unique<Session>(manager, stopped, 1));
        manager.AddStaticModule("ResidentConsumerSession", std::make_unique<Session>(manager, stopped, 2));
        manager.RegisterModule({"ResidentConsumerSession", {"ResidentProviderSession"}});
        manager.TopoSort();
        ASSERT_TRUE(manager.LoadModules({}));
        ASSERT_TRUE(manager.LoadModules({}));
        EXPECT_TRUE(manager.IsModuleInitialized("ResidentProviderSession"));
        EXPECT_TRUE(manager.IsModuleInitialized("ResidentConsumerSession"));
    }
    EXPECT_EQ(stopped, (std::vector<int>{2, -2, 1, -1}));
}

TEST(ModuleManagerStaticTest, UninitializedStaticInstanceReceivesNoServiceCallbacks)
{
    int         callbacks = 0;
    struct Cold final : sky::IModule {
        explicit Cold(int &callbacks) : callbacks(callbacks)
        {
        }
        void Start() override
        {
            ++callbacks;
        }
        void PreTick(float) override
        {
            ++callbacks;
        }
        void Tick(float) override
        {
            ++callbacks;
        }
        void PostTick(float) override
        {
            ++callbacks;
        }
        void Shutdown() override
        {
            ++callbacks;
        }
        int &callbacks;
    };
    {
        sky::ModuleManager manager;
        manager.AddStaticModule("UninitializedResidentSession", std::make_unique<Cold>(callbacks));
        manager.TopoSort();
        manager.StartModules();
        manager.Tick(0.f);
        EXPECT_FALSE(manager.IsModuleInitialized("UninitializedResidentSession"));
    }
    EXPECT_EQ(callbacks, 0);
}

TEST(ModuleManagerStaticTest, InitializationRejectsNestedLoadWithoutPublishingASuccessfulSession)
{
    sky::ModuleManager manager;
    int                calls             = 0;
    int                providerCallbacks = 0;
    struct Provider    final : sky::IModule {
        explicit Provider(int &calls) : calls(calls)
        {
        }
        void Start() override
        {
            ++calls;
        }
        void Tick(float) override
        {
            ++calls;
        }
        int &calls;
    };
    struct Initializer final : sky::IModule {
        Initializer(sky::ModuleManager &manager, int &calls, int &providerCallbacks)
            : manager(manager), calls(calls), providerCallbacks(providerCallbacks)
        {
        }
        bool Init(const sky::StartArguments &) override
        {
            ++calls;
            EXPECT_FALSE(manager.IsModuleInitialized("NonReentrantResidentInit"));
            EXPECT_FALSE(manager.LoadModules({}));
            EXPECT_FALSE(manager.UnLoadModules());
            EXPECT_FALSE(manager.RegisterModule({"InitializationRegistration", {}}));
            EXPECT_FALSE(manager.AddStaticModule("InitializationStaticRegistration", std::make_unique<sky::IModule>()));
            EXPECT_FALSE(manager.TopoSort());
            EXPECT_FALSE(manager.StartModules());
            EXPECT_FALSE(manager.Tick(0.f));
            EXPECT_EQ(providerCallbacks, 0);
            return true;
        }
        sky::ModuleManager &manager;
        int                &calls;
        int                &providerCallbacks;
    };
    manager.AddStaticModule("NonReentrantResidentProvider", std::make_unique<Provider>(providerCallbacks));
    manager.AddStaticModule("NonReentrantResidentInit", std::make_unique<Initializer>(manager, calls, providerCallbacks));
    manager.RegisterModule({"NonReentrantResidentInit", {"NonReentrantResidentProvider"}});
    manager.TopoSort();
    ASSERT_TRUE(manager.LoadModules({}));
    EXPECT_TRUE(manager.IsModuleInitialized("NonReentrantResidentInit"));
    EXPECT_EQ(calls, 1);
    manager.StartModules();
    manager.Tick(0.f);
    EXPECT_EQ(providerCallbacks, 2);
}

TEST(ModuleManagerStaticTest, ServiceCallbacksCannotReloadOrDestroyTheirExecutingSession)
{
    struct State {
        int  callbacks = 0;
        bool attempted = false;
        bool destroyed = false;
    };
    struct Session final : sky::IModule {
        Session(sky::ModuleManager &manager, State &state, int phase) : manager(manager), state(state), phase(phase)
        {
        }
        ~Session() override
        {
            state.destroyed = true;
        }
        void Start() override
        {
            Dispatch(0);
        }
        void PreTick(float) override
        {
            Dispatch(1);
        }
        void Tick(float) override
        {
            Dispatch(2);
        }
        void PostTick(float) override
        {
            Dispatch(3);
        }
        void Dispatch(int current)
        {
            ++state.callbacks;
            if (current != phase || state.attempted)
                return;
            state.attempted = true;
            // Keep observations outside the object even if the old implementation destroys it here.
            auto      *host     = &manager;
            auto      *observed = &state;
            const auto calls    = observed->callbacks;
            EXPECT_FALSE(host->StartModules());
            EXPECT_FALSE(host->Tick(0.f));
            EXPECT_EQ(observed->callbacks, calls);
            EXPECT_FALSE(host->RegisterModule({"CallbackRegistration", {}}));
            EXPECT_FALSE(host->AddStaticModule("CallbackStaticRegistration", std::make_unique<sky::IModule>()));
            EXPECT_FALSE(host->TopoSort());
            EXPECT_FALSE(host->LoadModules({}));
            EXPECT_FALSE(host->UnLoadModules());
            EXPECT_FALSE(observed->destroyed);
        }
        sky::ModuleManager &manager;
        State              &state;
        int                 phase;
    };
    for (int phase = 0; phase != 4; ++phase) {
        State              state;
        sky::ModuleManager manager;
        manager.AddStaticModule("ExecutingResidentSession", std::make_unique<Session>(manager, state, phase));
        manager.TopoSort();
        ASSERT_TRUE(manager.LoadModules({}));
        if (phase == 0)
            EXPECT_TRUE(manager.StartModules());
        else
            EXPECT_TRUE(manager.Tick(0.f));
        EXPECT_TRUE(state.attempted);
        EXPECT_FALSE(state.destroyed);
        EXPECT_TRUE(manager.UnLoadModules());
        EXPECT_TRUE(state.destroyed);
    }
}

TEST(ModuleManagerStaticTest, FailedInitializationShutsDownBeforeDestroyAndNeverStartsOrTicks)
{
    struct Counts {
        uint32_t stops = 0, destroys = 0, starts = 0, ticks = 0;
    } counts;
    class FailedModule final : public sky::IModule {
    public:
        explicit FailedModule(Counts &counts) : counts(counts)
        {
        }
        ~FailedModule() override
        {
            EXPECT_EQ(counts.stops, 1u);
            ++counts.destroys;
        }
        bool Init(const sky::StartArguments &) override
        {
            return false;
        }
        void Shutdown() override
        {
            ++counts.stops;
        }
        void Start() override
        {
            ++counts.starts;
        }
        void Tick(float) override
        {
            ++counts.ticks;
        }
        Counts &counts;
    };
    sky::ModuleManager manager;
    manager.AddStaticModule("FailedModule", std::make_unique<FailedModule>(counts));
    manager.TopoSort();
    EXPECT_FALSE(manager.LoadModules({}));
    EXPECT_EQ(counts.destroys, 1u);
    manager.StartModules();
    manager.Tick(0);
    manager.UnLoadModules();
    EXPECT_EQ(counts.starts, 0u);
    EXPECT_EQ(counts.ticks, 0u);
    EXPECT_EQ(counts.stops, 1u);
}

TEST(ModuleManagerStaticTest, B14WindowListenerCanDisconnectOneSurfaceWithoutAffectingAnother)
{
    struct Listener final : sky::IWindowEvent {
        uint32_t calls = 0;
        void     OnWindowClose(const sky::NativeWindow *) override
        {
            ++calls;
        }
    } listener;
    const auto *first  = reinterpret_cast<const sky::NativeWindow *>(uintptr_t{1});
    const auto *second = reinterpret_cast<const sky::NativeWindow *>(uintptr_t{2});
    sky::Event<sky::IWindowEvent>::Connect(first, &listener);
    sky::Event<sky::IWindowEvent>::Connect(second, &listener);
    sky::Event<sky::IWindowEvent>::DisConnect(first, &listener);
    sky::Event<sky::IWindowEvent>::BroadCast(first, &sky::IWindowEvent::OnWindowClose, first);
    sky::Event<sky::IWindowEvent>::BroadCast(second, &sky::IWindowEvent::OnWindowClose, second);
    EXPECT_EQ(listener.calls, 1u);
    sky::Event<sky::IWindowEvent>::DisConnect(second, &listener);
}

TEST(ModuleManagerStaticTest, B14FailedDependencyPreventsConsumerInitialization)
{
    struct Failed final : sky::IModule {
        bool Init(const sky::StartArguments &) override
        {
            return false;
        }
    };
    struct Consumer final : sky::IModule {
        explicit Consumer(bool &called) : called(called)
        {
        }
        bool Init(const sky::StartArguments &) override
        {
            called = true;
            return true;
        }
        bool &called;
    };
    bool               called = false;
    sky::ModuleManager manager;
    manager.AddStaticModule("provider", std::make_unique<Failed>());
    manager.AddStaticModule("consumer", std::make_unique<Consumer>(called));
    manager.RegisterModule({"consumer", {"provider"}});
    manager.TopoSort();
    EXPECT_FALSE(manager.LoadModules({}));
    EXPECT_FALSE(called);
    EXPECT_FALSE(manager.IsModuleInitialized("consumer"));
    manager.UnLoadModules();
}

TEST(ModuleManagerStaticTest, FailedInitRetryKeepsMetadataProcessOnceAndCleansEachInstance)
{
    struct Counts {
        uint32_t metadata = 0, inits = 0, stops = 0, destroys = 0;
    } counts;
    class RetryModule final : public sky::IModule {
    public:
        explicit RetryModule(Counts &counts) : counts(counts)
        {
        }
        ~RetryModule() override
        {
            ++counts.destroys;
        }
        void RegisterMeta() override
        {
            ++counts.metadata;
        }
        bool Init(const sky::StartArguments &) override
        {
            EXPECT_EQ(counts.metadata, 1u);
            return ++counts.inits > 1;
        }
        void Shutdown() override
        {
            ++counts.stops;
        }
        Counts &counts;
    };
    for (uint32_t attempt = 0; attempt < 2; ++attempt) {
        sky::ModuleManager manager;
        manager.AddStaticModule("MetadataRetryModule", std::make_unique<RetryModule>(counts));
        manager.TopoSort();
        EXPECT_EQ(manager.LoadModules({}), attempt != 0);
        EXPECT_EQ(manager.IsModuleInitialized("MetadataRetryModule"), attempt != 0);
        manager.UnLoadModules();
        EXPECT_FALSE(manager.IsModuleInitialized("MetadataRetryModule"));
        EXPECT_EQ(counts.stops, attempt + 1);
        EXPECT_EQ(counts.destroys, attempt + 1);
    }
    EXPECT_EQ(counts.metadata, 1u);
}
