//
// Aurora launcher producer; native command ownership stays with IRenderer.
//

#include <aurora/adaptor/AuroraModule.h>

#include <array>
#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/renderer/RendererProvider.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <core/cmdline/CmdParser.h>
#include <core/logger/Logger.h>
#include <core/name/Name.h>
#include <framework/application/ModuleManager.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/platform/PlatformBase.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/window/NativeWindowManager.h>

static const char *TAG = "AuroraModule";

namespace sky::aurora {

    AuroraModule::AuroraModule() = default;

    AuroraModule::~AuroraModule()
    {
        Shutdown();
    }

    namespace {
        API ParseApi(const std::string &name)
        {
            if (name == "vulkan" || name == "vk") {
                return API::VULKAN;
            }
            if (name == "dx12" || name == "d3d12") {
                return API::DX12;
            }
            if (name == "metal") {
                return API::METAL;
            }
            return API::DEFAULT;
        }
    } // namespace

    void AuroraModule::ProcessArgs(const StartArguments &args)
    {
        mApi         = API::DEFAULT;
        providerOnly = false;
        vulkanMode   = RendererVulkanMode::DYNAMIC_RENDERING;
        CmdOptions options("Aurora Render Module", "Aurora Render Module");
        options.allow_unrecognised_options();
        options.add_options()("r,rhi", "RHI Type", CmdValue<std::string>());
        options.add_options()("aurora-provider-only", "Register the Renderer provider without creating a host");
        options.add_options()("aurora-vulkan-render-pass", "Use the Vulkan render-pass compiler");

        if (args.args.empty()) {
            return;
        }

        auto result  = options.parse(static_cast<int32_t>(args.args.size()), args.args.data());
        providerOnly = result.count("aurora-provider-only") != 0u;
        vulkanMode   = result.count("aurora-vulkan-render-pass") ? RendererVulkanMode::RENDER_PASS : RendererVulkanMode::DYNAMIC_RENDERING;
        if (result.count("rhi") != 0u) {
            mApi = ParseApi(result["rhi"].as<std::string>());
        }
    }

    void AuroraModule::RegisterMeta()
    {
        // Other Aurora modules share this process-wide reflection registration.
        AuroraReflection(SerializationContext::Get());
    }

    bool AuroraModule::Init(const StartArguments &args)
    {
        auto *service = Interface<IRendererProvider>::Get();
        if (provider) {
            return service->GetApi() == provider.get();
        }
        if (service->GetApi() != nullptr) {
            return false;
        }
        ProcessArgs(args);

        provider = CreateRendererProvider();
        if (providerOnly) {
            AuroraReflection(SerializationContext::Get());
            service->Register(*provider);
            return true;
        }
        RendererInitInfo info;
        info.backend    = mApi;
        info.vulkanMode = vulkanMode;
#if defined(_DEBUG)
        info.enableValidation = true;
#endif
        if (const auto error = host.Init(*provider, info)) {
            Fail(*error);
            Shutdown();
            return false;
        }
        const auto active = host.Get()->GetInfo();
        LOG_I(TAG, "Renderer host ready: backend=%u epoch=%llu", static_cast<unsigned>(active.backend), active.deviceEpoch);
        AuroraReflection(SerializationContext::Get());
        service->Register(*provider);
        return true;
    }

    void AuroraModule::Start()
    {
        if (!host.Get() || started)
            return;
        void *handle = nullptr;
        if (auto *system = Interface<ISystemNotify>::Get()->GetApi())
            handle = system->GetMainWindowHandle();
        window = NativeWindowManager::Get()->GetMainWindow();
        if (!handle && Platform::Get()->GetImpl())
            handle = Platform::Get()->GetMainWinHandle();
        if (!handle) {
            Fail({RendererErrorCode::NOT_READY, "Launcher requires a main window"});
            return;
        }
        if (window && window->GetNativeHandle() == handle) {
            extent = {window->GetWidth(), window->GetHeight()};
            window->SetCloseDeferred(true);
            Event<IWindowEvent>::Connect(window, this);
        } else {
            window = nullptr;
        }
        pending                = std::make_unique<FramePacket>();
        pending->sequence      = ++sequence;
        auto       *system     = Interface<ISystemNotify>::Get()->GetApi();
        auto       *modules    = system ? system->GetModuleManager() : nullptr;
        auto        sources    = std::make_shared<ShaderFileSystem>();
        auto        products   = Platform::Get()->GetBundleFileSystem()->CreateSubSystem("products/shaders", false);
        auto        sourceRoot = products ? FileSystemPtr{} : Platform::Get()->GetBundleFileSystem()->CreateSubSystem("assets/shaders", false);
        std::string sourceError;
        if (!products && (!sourceRoot || !sources->LoadBundles(*sourceRoot, "", &sourceError))) {
            Fail({RendererErrorCode::INVALID_ARGUMENT, "Shader source registration failed: " + sourceError});
            return;
        }
        if (const auto error = RegisterScenePass(*pending, {0, 1}, modules ? modules->RetainModule("Aurora.Render") : nullptr, std::move(sources),
                                                 std::move(products))) {
            Fail(*error);
            return;
        }
        if (const auto error = scene.Create(*pending, {0, 1}, {Name("main")})) {
            Fail(*error);
            return;
        }
        ViewportDesc output;
        output.kind         = ViewportKind::NATIVE_WINDOW;
        output.extent       = extent;
        output.format       = PixelFormat::BGRA8_UNORM;
        output.nativeWindow = reinterpret_cast<uintptr_t>(handle);
        pending->commands.push_back(CreateViewport{{0, 1}, output});
        ViewDesc view;
        view.scene  = scene.GetSceneId();
        view.output = {0, 1};
        pending->commands.push_back(CreateView{{0, 1}, view});
        submittedExtent = extent;
        started         = true;
    }

    void AuroraModule::Fail(const RendererError &error)
    {
        LOG_E(TAG, "renderer host failed: %s", error.message.c_str());
        pending.reset();
        if (host.Get())
            host.Get()->RequestShutdown();
        if (auto *system = Interface<ISystemNotify>::Get()->GetApi())
            system->SetExit();
    }

    void AuroraModule::PollEvents()
    {
        std::array<RendererEvent, 32> events;
        size_t                        count = 0;
        do {
            count = host.Get()->PollEvents(events);
            for (const auto &event : std::span(events).first(count)) {
                scene.OnEvent(event);
                if (const auto *failure = std::get_if<RendererFailure>(&event))
                    Fail(failure->error);
                if (const auto *failure = std::get_if<ViewportFailed>(&event))
                    LOG_W(TAG, "viewport %u: %s", failure->viewport.index, failure->error.message.c_str());
            }
        } while (count == events.size());
    }

    void AuroraModule::Tick(float delta)
    {
        if (!host.Get())
            return;
        PollEvents();
        if (!started || host.Get()->GetState() != RendererState::RUNNING)
            return;
        if (pending) {
            const auto result = host.Get()->TrySubmit(pending);
            if (result.status == PacketSubmitStatus::REJECTED)
                Fail(result.error);
            if (result.status != PacketSubmitStatus::ACCEPTED)
                return;
        }
        if (retiring)
            return;
        pending               = std::make_unique<FramePacket>();
        pending->sequence     = ++sequence;
        pending->deltaSeconds = delta;
        if (!closed && sceneSource)
            sceneSource(*pending, scene.GetSceneId());
        if (closed) {
            pending->commands.push_back(DestroyView{{0, 1}});
            pending->commands.push_back(DestroyViewport{{0, 1}});
            if (const auto error = scene.Destroy(*pending)) {
                Fail(*error);
                return;
            }
            retiring = true;
        } else if (extent.width != submittedExtent.width || extent.height != submittedExtent.height) {
            pending->commands.push_back(ResizeViewport{{0, 1}, extent, ++revision});
            submittedExtent = extent;
        }
        const auto result = host.Get()->TrySubmit(pending);
        if (result.status == PacketSubmitStatus::REJECTED)
            Fail(result.error);
    }

    void AuroraModule::OnWindowResize(const WindowResizeEvent &event)
    {
        if (window && window->GetWinId() == event.winID)
            extent = {event.width, event.height};
    }

    void AuroraModule::OnWindowClose(const NativeWindow *closing)
    {
        if (closing == window)
            closed = true;
    }

    void AuroraModule::Shutdown()
    {
        const bool active = provider || host.Get();
        Event<IWindowEvent>::DisConnect(this);
        pending.reset();
        host.Shutdown();
        scene.OnRendererStopped();
        if (window)
            window->SetCloseDeferred(false);
        window = nullptr;
        if (provider) {
            auto *service = Interface<IRendererProvider>::Get();
            if (service->GetApi() == provider.get())
                service->UnRegister();
            provider.reset();
        }
        sceneSource = {};
        started = closed = retiring = false;
        sequence = revision = 0;
        if (active)
            LOG_I(TAG, "Renderer host stopped");
    }

} // namespace sky::aurora
