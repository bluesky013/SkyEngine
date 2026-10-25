//
// ModuleManager: statically-registered (compiled-in) modules load without dlopen.
//

#include <framework/application/ModuleManager.h>
#include <framework/interface/IModule.h>

#include <framework/window/IWindowEvent.h>
#include <gtest/gtest.h>

#include <memory>

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
    EXPECT_FALSE(manager.RetainModule("consumer"));
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
        EXPECT_EQ(bool(manager.RetainModule("MetadataRetryModule")), attempt != 0);
        manager.UnLoadModules();
        EXPECT_FALSE(manager.RetainModule("MetadataRetryModule"));
        EXPECT_EQ(counts.stops, attempt + 1);
        EXPECT_EQ(counts.destroys, attempt + 1);
    }
    EXPECT_EQ(counts.metadata, 1u);
}
