#include <array>
#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/renderer/IRenderer.h>
#include <aurora/rhi/Instance.h>
#include <chrono>
#include <framework/application/ModuleManager.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/platform/PlatformBase.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/window/NativeWindow.h>
#include <gtest/gtest.h>
#include <thread>

#if defined(_WIN32)
    #include <windows.h>
#endif

using namespace sky;
using namespace sky::aurora;

#if defined(AURORA_RENDERER_STATIC_FIXTURE)
extern "C" IModule *sky_module_create_Aurora_Render_Contract();
#elif defined(AURORA_RENDERER_PACKAGED_STATIC)
extern "C" IModule *sky_module_create_Aurora_Render();
#endif

namespace {
    using ProviderService = Interface<IRendererProvider>;

    struct LoadedProvider {
        ModuleManager manager;
        explicit LoadedProvider(bool runtime = false)
        {
#if defined(AURORA_RENDERER_STATIC_FIXTURE)
            manager.AddStaticModule("Aurora.Render", std::unique_ptr<IModule>(sky_module_create_Aurora_Render_Contract()));
#elif defined(AURORA_RENDERER_PACKAGED_STATIC)
            manager.AddStaticModule("Aurora.Render", std::unique_ptr<IModule>(sky_module_create_Aurora_Render()));
#else
            manager.RegisterModule({"Aurora.Render", {}});
#endif
            manager.TopoSort();
            StartArguments args;
            if (runtime)
                args.args = {"RendererModuleTest", "--aurora-provider-only"};
            manager.LoadModules(args);
        }
        ~LoadedProvider()
        {
            manager.UnLoadModules();
        }
    };
} // namespace

TEST(RendererModule, RegistersInHostEnvironmentAndReloads)
{
    // Obtain the service in the host BEFORE dlopen: catches per-DLL singletons.
    auto *service = ProviderService::Get();
    ASSERT_EQ(service->GetApi(), nullptr);
    for (int iteration = 0; iteration < 2; ++iteration) {
        {
            LoadedProvider loaded;
            auto          *provider = service->GetApi();
            ASSERT_NE(provider, nullptr);
            auto result = provider->Create({});
            ASSERT_TRUE(std::holds_alternative<RendererError>(result));
            EXPECT_EQ(std::get<RendererError>(result).code, RendererErrorCode::NOT_READY);
            EXPECT_FALSE(std::get<RendererError>(result).message.empty());
        }
        EXPECT_EQ(service->GetApi(), nullptr);
    }
}

TEST(RendererModule, RejectsZeroCountsAndBudgets)
{
    LoadedProvider loaded;
    auto          *provider = ProviderService::Get()->GetApi();
    ASSERT_NE(provider, nullptr);
    for (auto member : {&RendererInitInfo::inflightFrames, &RendererInitInfo::maxPendingPackets, &RendererInitInfo::prepareWorkers,
                        &RendererInitInfo::encodeWorkers}) {
        RendererInitInfo info;
        info.*member = 0;
        auto result  = provider->Create(info);
        ASSERT_TRUE(std::holds_alternative<RendererError>(result));
        EXPECT_EQ(std::get<RendererError>(result).code, RendererErrorCode::INVALID_ARGUMENT);
    }
    for (auto member : {&RendererInitInfo::maxPendingResourceBytes, &RendererInitInfo::maxUploadBytesPerFrame}) {
        RendererInitInfo info;
        info.*member = 0;
        auto result  = provider->Create(info);
        ASSERT_TRUE(std::holds_alternative<RendererError>(result));
        EXPECT_EQ(std::get<RendererError>(result).code, RendererErrorCode::INVALID_ARGUMENT);
    }
}

TEST(RendererModule, RuntimeServiceDrainsBeforeUnloadAndCanReload)
{
    Instance::Destroy();
    Instance::Get();
    uint64_t previousEpoch = 0;
    for (uint32_t iteration = 0; iteration < 2; ++iteration) {
        LoadedProvider loaded(true);
        auto          *provider = ProviderService::Get()->GetApi();
        ASSERT_TRUE(provider);
        auto created = provider->Create({});
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<IRenderer>>(created));
        auto renderer = std::move(std::get<std::unique_ptr<IRenderer>>(created));
        EXPECT_GT(renderer->GetInfo().deviceEpoch, previousEpoch);
        previousEpoch    = renderer->GetInfo().deviceEpoch;
        auto packet      = std::make_unique<FramePacket>();
        packet->sequence = 1;
        packet->commands.push_back(CreateScene{{0, 1}, {}});
        ASSERT_EQ(renderer->TrySubmit(packet).status, PacketSubmitStatus::ACCEPTED);
        renderer->RequestShutdown();
        bool       stopped = false, applied = false, completed = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!stopped && std::chrono::steady_clock::now() < deadline) {
            std::array<RendererEvent, 8> events;
            const auto                   count = renderer->PollEvents(events);
            for (const auto &event : std::span(events).first(count)) {
                applied |= std::holds_alternative<PacketApplied>(event);
                completed |= std::holds_alternative<FrameCompleted>(event);
                stopped |= std::holds_alternative<RendererStopped>(event);
                EXPECT_FALSE(std::holds_alternative<RendererFailure>(event));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        EXPECT_TRUE(stopped && applied && completed);
        renderer.reset();
    }
}

#if defined(_WIN32)
TEST(RendererModule, NativeLauncherDrainsBeforeLoadedModuleAndWindowAreReleased)
{
    struct HostWindow final : ISystemNotify {
        ~HostWindow() override
        {
            Interface<ISystemNotify>::Get()->UnRegister();
            window.reset();
            Platform::Get()->Shutdown();
        }
        void SetExit() override
        {
            failed = true;
        }
        void *GetMainWindowHandle() const override
        {
            return window->GetNativeHandle();
        }
        std::unique_ptr<NativeWindow> window;
        bool                          failed = false;
    } host;
    ASSERT_TRUE(Platform::Get()->Init({}));
    host.window.reset(NativeWindow::Create({128, 96, "AuroraB14Module", "Aurora module host test", nullptr}));
    ASSERT_TRUE(host.window);
    Interface<ISystemNotify>::Get()->Register(host);
    const auto handle = static_cast<HWND>(host.window->GetNativeHandle());
    {
        LoadedProvider loaded;
        auto          *renderer = Interface<IRenderer>::Get()->GetApi();
        ASSERT_TRUE(renderer);
        loaded.manager.StartModules();
        EXPECT_TRUE(host.window->IsCloseDeferred());
        loaded.manager.Tick(1.f / 60);
        bool       complete = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!complete && std::chrono::steady_clock::now() < deadline) {
            bool quit = false;
            Platform::Get()->PoolEvent(quit);
            std::array<RendererEvent, 16> events;
            const auto                    count = renderer->PollEvents(events);
            for (const auto &event : std::span(events).first(count)) {
                if (const auto *frame = std::get_if<FrameCompleted>(&event))
                    complete |= frame->submissionSerial != 0;
                EXPECT_FALSE(std::holds_alternative<RendererFailure>(event));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        EXPECT_TRUE(complete);
        // Leave an accepted frame for module shutdown to drain while the window still exists.
        loaded.manager.Tick(1.f / 60);
    }
    EXPECT_EQ(Interface<IRenderer>::Get()->GetApi(), nullptr);
    EXPECT_EQ(Interface<IRendererProvider>::Get()->GetApi(), nullptr);
    EXPECT_TRUE(IsWindow(handle));
    EXPECT_FALSE(host.window->IsCloseDeferred());
    EXPECT_FALSE(host.failed);
}
#endif

#if defined(AURORA_RENDERER_STATIC_FIXTURE)
TEST(RendererModule, DuplicateRegistrationDoesNotReplaceOrUnregisterOwner)
{
    auto owner     = std::unique_ptr<IModule>(sky_module_create_Aurora_Render_Contract());
    auto duplicate = std::unique_ptr<IModule>(sky_module_create_Aurora_Render_Contract());
    ASSERT_TRUE(owner->Init({}));
    auto *provider = ProviderService::Get()->GetApi();
    EXPECT_TRUE(owner->Init({}));
    EXPECT_FALSE(duplicate->Init({}));
    duplicate->Shutdown();
    EXPECT_EQ(ProviderService::Get()->GetApi(), provider);
    owner->Shutdown();
    EXPECT_EQ(ProviderService::Get()->GetApi(), nullptr);
    owner->Shutdown();
    EXPECT_EQ(ProviderService::Get()->GetApi(), nullptr);
}
#endif

TEST(RendererModule, B14LeaseRetainsModuleCodeBeyondManagerLifetime)
{
    std::shared_ptr<const void> lease;
    std::weak_ptr<const void>   weak;
    {
        LoadedProvider loaded(true);
        lease = loaded.manager.RetainModule("Aurora.Render");
        ASSERT_TRUE(lease);
        weak = lease;
        EXPECT_FALSE(loaded.manager.RetainModule("missing"));
        loaded.manager.UnLoadModules();
        EXPECT_FALSE(loaded.manager.RetainModule("Aurora.Render"));
        EXPECT_EQ(ProviderService::Get()->GetApi(), nullptr);
    }
    EXPECT_FALSE(weak.expired());
#if defined(_WIN32) && !defined(AURORA_RENDERER_STATIC_FIXTURE) && !defined(AURORA_RENDERER_PACKAGED_STATIC)
    EXPECT_NE(GetModuleHandleA("Aurora.Render.dll"), nullptr);
#endif
    lease.reset();
    EXPECT_TRUE(weak.expired());
#if defined(_WIN32) && !defined(AURORA_RENDERER_STATIC_FIXTURE) && !defined(AURORA_RENDERER_PACKAGED_STATIC)
    EXPECT_EQ(GetModuleHandleA("Aurora.Render.dll"), nullptr);
#endif
}

int main(int argc, char **argv)
{
    // Shared device and reflection callbacks must outlive the reloadable module.
    Instance::Get();
    AuroraReflection(SerializationContext::Get());
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
