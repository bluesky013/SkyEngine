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

    AuroraModule::AuroraModule(std::unique_ptr<IRendererProvider> source, FileSystemPtr root) : bundle(std::move(root)), provider(std::move(source))
    {
    }

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
        if (service->GetApi() != nullptr) {
            return service->GetApi() == provider.get();
        }
        ProcessArgs(args);

        if (!provider)
            provider = CreateRendererProvider();
        if (providerOnly) {
            AuroraReflection(SerializationContext::Get());
            service->Register(*provider);
            return true;
        }
        RendererInitInfo info;
        info.backend    = mApi;
        info.vulkanMode = vulkanMode;
#if SKY_ENABLE_GPU_VALIDATION
        info.enableValidation = true;
#endif
        if (const auto error = host.Init(*provider, info)) {
            Fail(*error);
            Shutdown();
            return false;
        }
        const auto active = host.Get()->GetInfo();
        LOG_I(TAG, "Renderer host ready: backend=%u epoch=%llu", static_cast<unsigned>(active.backend),
              static_cast<unsigned long long>(active.deviceEpoch));
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
        if (!handle && !window) {
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
        desiredSurface      = reinterpret_cast<uintptr_t>(handle);
        desiredSurfaceLease = window ? window->RetainNativeHandle() : nullptr;
        pending             = std::make_unique<FramePacket>();
        pending->sequence   = ++sequence;
        auto *system        = Interface<ISystemNotify>::Get()->GetApi();
        auto *modules       = system ? system->GetModuleManager() : nullptr;
        auto  root          = bundle;
        if (!root && Platform::Get()->GetImpl())
            root = Platform::Get()->GetBundleFileSystem();
        if (!root) {
            Fail({RendererErrorCode::INVALID_ARGUMENT, "Shader resource bundle is unavailable"});
            return;
        }
#if SKY_DEVELOP
        constexpr auto mode = ShaderResourceMode::DEVELOPMENT;
#else
        constexpr auto mode = ShaderResourceMode::PRODUCTS;
#endif
        auto resources = LoadShaderResources(*root, host.Get()->GetInfo().backend, mode);
        if (const auto *error = std::get_if<std::string>(&resources)) {
            Fail({RendererErrorCode::INVALID_ARGUMENT, "Shader resource registration failed: " + *error});
            return;
        }
        auto &shaders = std::get<ShaderResources>(resources);
        if (const auto error = RegisterScenePass(*pending, {0, 1}, modules ? modules->RetainModule("Aurora.Render") : nullptr,
                                                 std::move(shaders.sources), std::move(shaders.products))) {
            Fail(*error);
            return;
        }
        if (const auto error = scene.Create(*pending, {0, 1}, {Name("main")})) {
            Fail(*error);
            return;
        }
        UpdateOutput(*pending);
        started = true;
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
                if (const auto *failure = std::get_if<ViewportFailed>(&event); failure && failure->viewport == viewport) {
                    if (failure->requiresReplacement)
                        replaceOutput = true;
                    else
                        LOG_W(TAG, "viewport %u unavailable: %s", failure->viewport.index, failure->error.message.c_str());
                }
                if (const auto *destroyed = std::get_if<ViewportDestroyed>(&event);
                    destroyed && destroyed->viewport == viewport && outputState == OutputState::RETIRING) {
                    activeSurfaceLease.reset();
                    activeSurface = 0;
                    outputState   = OutputState::ABSENT;
                    if (closed && window)
                        window->SetCloseDeferred(false);
                    if (viewport.generation == UINT32_MAX)
                        Fail({RendererErrorCode::OUT_OF_MEMORY, "Launcher viewport generation exhausted"});
                    else
                        ++viewport.generation;
                }
            }
        } while (count == events.size());
    }

    void AuroraModule::Tick(float delta)
    {
        Pump(delta, true);
    }

    void AuroraModule::UpdateOutput(FramePacket &packet)
    {
        if (outputState == OutputState::LIVE && (closed || replaceOutput || desiredSurface != activeSurface)) {
            packet.commands.push_back(DestroyViewport{viewport});
            outputState   = OutputState::RETIRING;
            replaceOutput = false;
        }
        if (closed || outputState != OutputState::ABSENT || !desiredSurface)
            return;
        ViewportDesc output;
        output.kind         = ViewportKind::NATIVE_WINDOW;
        output.extent       = extent;
        output.format       = PixelFormat::BGRA8_UNORM;
        output.nativeWindow = desiredSurface;
        activeSurface       = desiredSurface;
        activeSurfaceLease  = desiredSurfaceLease;
        packet.commands.push_back(CreateViewport{viewport, output});
        viewDescription.scene  = scene.GetSceneId();
        viewDescription.output = viewport;
        if (viewCreated)
            packet.commands.push_back(UpdateView{view, viewDescription, 0});
        else {
            packet.commands.push_back(CreateView{view, viewDescription});
            viewCreated = true;
        }
        outputState     = OutputState::LIVE;
        replaceOutput   = false;
        submittedExtent = extent;
        revision        = 0;
    }

    void AuroraModule::Pump(float delta, bool produceScene)
    {
        if (pumping || !host.Get())
            return;
        pumping = true;
        struct PumpGuard {
            bool &active;
            ~PumpGuard()
            {
                active = false;
            }
        } guard{pumping};
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
        UpdateOutput(*pending);
        if (!closed && produceScene && sceneSource)
            sceneSource(*pending, scene.GetSceneId());
        // Keep the owned main view's CPU settings while its output is absent.
        std::erase_if(pending->commands, [&](RenderCommand &command) {
            auto *update = std::get_if<UpdateView>(&command);
            if (!update || update->id != view)
                return false;
            viewDescription        = update->desc;
            viewDescription.output = viewport;
            if (outputState != OutputState::LIVE)
                return true;
            update->desc     = viewDescription;
            update->revision = ++viewRevision;
            return false;
        });
        std::erase_if(pending->extensions, [&](FrameExtensionPayload &extension) {
            for (auto &output : extension.outputs) {
                if (!output.IsValid() || output.index != viewport.index || output.generation > viewport.generation)
                    continue;
                if (outputState != OutputState::LIVE)
                    return true;
                output = viewport;
            }
            return false;
        });
        if (closed) {
            UpdateOutput(*pending);
            if (viewCreated)
                pending->commands.push_back(DestroyView{view});
            if (const auto error = scene.Destroy(*pending)) {
                Fail(*error);
                return;
            }
            retiring = true;
        } else if (outputState == OutputState::LIVE && (extent.width != submittedExtent.width || extent.height != submittedExtent.height)) {
            pending->commands.push_back(ResizeViewport{viewport, extent, ++revision});
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
        if (closing && closing == window) {
            closed         = true;
            desiredSurface = 0;
            desiredSurfaceLease.reset();
            Pump(0, false);
        }
    }

    void AuroraModule::OnSurfaceChanged(const NativeWindow *changed)
    {
        if (!changed || changed != window || closed)
            return;
        desiredSurface      = reinterpret_cast<uintptr_t>(changed->GetNativeHandle());
        desiredSurfaceLease = changed->RetainNativeHandle();
        extent              = {changed->GetWidth(), changed->GetHeight()};
        replaceOutput       = outputState == OutputState::LIVE;
        Pump(0, false);
    }

    void AuroraModule::Shutdown()
    {
        const bool active = provider || host.Get();
        Event<IWindowEvent>::DisConnect(this);
        pending.reset();
        host.Shutdown();
        activeSurfaceLease.reset();
        desiredSurfaceLease.reset();
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
        activeSurface = desiredSurface = 0;
        outputState                    = OutputState::ABSENT;
        viewport                       = {0, 1};
        viewCreated = replaceOutput = false;
        viewRevision                = 0;
        viewDescription             = {};
        sequence = revision = 0;
        if (active)
            LOG_I(TAG, "Renderer host stopped");
    }

} // namespace sky::aurora
