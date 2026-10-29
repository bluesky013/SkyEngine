//
// Aurora launcher producer; native command ownership stays with IRenderer.
//

#include <aurora/adaptor/AuroraModule.h>

#include <array>
#include <aurora/adaptor/AnimationSkeletonSource.h>
#include <aurora/adaptor/renderer/RenderSceneBridge.h>
#include <aurora/adaptor/renderer/SceneAssetSource.h>
#include <aurora/renderer/RendererProvider.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderUsage.h>
#include <core/cmdline/CmdParser.h>
#include <core/logger/Logger.h>
#include <core/name/Name.h>
#include <framework/application/ModuleManager.h>
#include <framework/asset/AssetManager.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/platform/PlatformBase.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/window/NativeWindowManager.h>
#include <framework/world/IGameWorld.h>
#include <framework/world/World.h>
#include <utility>

static const char *TAG = "AuroraModule";

namespace sky::aurora {

    struct AuroraModule::SceneSession {
        LauncherSceneRegistration registration;
        // Destroy the session's DLL-defined state before releasing its code lease.
        std::shared_ptr<const void>            moduleLease;
        std::shared_ptr<ILauncherSceneSession> session;
        World                                 *defaultWorld = nullptr;
    };

    AuroraModule::AuroraModule() = default;

    AuroraModule::AuroraModule(std::unique_ptr<IRendererProvider> source, FileSystemPtr root) : bundle(std::move(root)), provider(std::move(source))
    {
    }

    AuroraModule::~AuroraModule()
    {
        Shutdown();
    }

    std::variant<RendererError, LauncherSceneRegistration> AuroraModule::RegisterSceneSession(std::string_view                       moduleName,
                                                                                              std::shared_ptr<ILauncherSceneSession> session)
    {
        if (std::this_thread::get_id() != sceneThread)
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Scene registration requires the launcher thread"};
        if (!host.Get() || providerOnly || closed || Interface<ILauncherSceneSource>::Get()->GetApi() != this)
            return RendererError{RendererErrorCode::NOT_READY, "Launcher scene service is unavailable"};
        if (!session || moduleName.empty())
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Scene registration requires a session and its defining module"};
        if (sceneSession && !sceneSession->defaultWorld)
            return RendererError{RendererErrorCode::NOT_READY, "A launcher scene session is already registered"};
        const auto *system  = Interface<ISystemNotify>::Get()->GetApi();
        const auto *modules = system ? system->GetModuleManager() : ModuleManager::Current();
        const auto  lease   = modules ? modules->RetainModule(std::string(moduleName)) : nullptr;
        if (!lease)
            return RendererError{RendererErrorCode::NOT_READY, "Scene defining module is not loaded"};
        if (sessionGeneration == UINT64_MAX)
            return RendererError{RendererErrorCode::OUT_OF_MEMORY, "Scene registration generation exhausted"};
        const LauncherSceneRegistration registration{sceneOwner, ++sessionGeneration};
        if (sceneSession)
            resetScene = started;
        sceneSession = std::make_shared<SceneSession>(SceneSession{registration, lease, std::move(session)});
        return registration;
    }

    std::optional<RendererError> AuroraModule::UnregisterSceneSession(LauncherSceneRegistration registration)
    {
        if (std::this_thread::get_id() != sceneThread)
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Scene removal requires the launcher thread"};
        if (!sceneSession || sceneSession->registration != registration)
            return RendererError{RendererErrorCode::STALE_HANDLE, "Scene registration does not own the active session"};
        sceneSession.reset();
        resetScene = started;
        return {};
    }

    namespace {
        class DefaultWorldSession final : public ILauncherSceneSession {
        public:
            DefaultWorldSession(World *world, RendererInfo info, std::shared_ptr<ISceneResourceSource> resources)
                : world(world), bridge(info, std::move(resources))
            {
                bridge.SetWorld(this->world.Get());
            }
            std::optional<RendererError> Append(FramePacket &packet, const LauncherSceneContext &context) override
            {
                if (const auto error = bridge.Append(packet, context.scene, context.output))
                    return error;
                if (!bridge.GetIssues().empty())
                    return bridge.GetIssues().front();
                if (!context.extent.width || !context.extent.height)
                    return {};
                const auto camera = bridge.Camera(context.scene, context.output, context.extent);
                if (!camera)
                    return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Default World requires a valid camera"};
                packet.commands.push_back(UpdateView{context.mainView, *camera, 0});
                return {};
            }
            void OnEvent(const RendererEvent &event) override
            {
                bridge.OnEvent(event);
            }

        private:
            WorldPtr          world;
            RenderSceneBridge bridge;
        };

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
        mApi            = API::DEFAULT;
        providerOnly    = false;
        vulkanMode      = RendererVulkanMode::AUTO;
        shaderUsagePath = {};
        CmdOptions options("Aurora Render Module", "Aurora Render Module");
        options.allow_unrecognised_options();
        options.add_options()("r,rhi", "RHI Type", CmdValue<std::string>());
        options.add_options()("aurora-provider-only", "Register the Renderer provider without creating a host");
        options.add_options()("aurora-vulkan-render-pass", "Use the Vulkan render-pass compiler");
        options.add_options()("aurora-shader-usage", "Write collected shader variants to this JSON path", CmdValue<std::string>());

        if (args.args.empty()) {
            return;
        }

        auto result  = options.parse(static_cast<int32_t>(args.args.size()), args.args.data());
        providerOnly = result.count("aurora-provider-only") != 0u;
        vulkanMode   = result.count("aurora-vulkan-render-pass") ? RendererVulkanMode::RENDER_PASS : RendererVulkanMode::AUTO;
        if (result.count("rhi") != 0u) {
            mApi = ParseApi(result["rhi"].as<std::string>());
        }
        if (result.count("aurora-shader-usage") != 0u) {
            shaderUsagePath = result["aurora-shader-usage"].as<std::string>();
        }
    }

    std::optional<RendererError> AuroraModule::UpdateDefaultSceneSession()
    {
        if (sceneSession && !sceneSession->defaultWorld)
            return {};
        const auto *service = Interface<IGameWorld>::Get()->GetApi();
        auto       *world   = service ? service->GetWorld() : nullptr;
        if (sceneSession && sceneSession->defaultWorld == world)
            return {};
        if (sceneSession) {
            if (const auto error = UnregisterSceneSession(sceneSession->registration))
                return error;
        }
        if (!world)
            return {};
        auto root = bundle;
        if (!root && Platform::Get()->GetImpl())
            root = Platform::Get()->GetBundleFileSystem();
        if (!root)
            return RendererError{RendererErrorCode::NOT_READY, "Default World shader bundle is unavailable"};
#if SKY_DEVELOP
        constexpr auto mode = ShaderResourceMode::DEVELOPMENT;
#else
        constexpr auto mode = ShaderResourceMode::PRODUCTS;
#endif
        auto resources = LoadShaderResources(*root, host.Get()->GetInfo().backend, mode);
        if (const auto *error = std::get_if<std::string>(&resources))
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Default World shader resources are unavailable: " + *error};
        auto &shaders = std::get<ShaderResources>(resources);
        auto  source  = CreateSceneAssetSource(
            std::move(shaders.sources), [](const Uuid &id) { return AssetManager::Get()->LoadAsset(id, AssetLoadPolicy::ProductsOnly); },
            std::move(shaders.products));
        if (!source)
            return RendererError{RendererErrorCode::NOT_READY, "Default World asset source is unavailable"};
        const auto registered =
            RegisterSceneSession("Aurora.Render", std::make_shared<DefaultWorldSession>(world, host.Get()->GetInfo(), std::move(source)));
        if (const auto *error = std::get_if<RendererError>(&registered))
            return *error;
        sceneSession->defaultWorld = world;
        return {};
    }

    bool AuroraModule::Init(const StartArguments &args)
    {
        auto *service = Interface<IRendererProvider>::Get();
        if (service->GetApi() != nullptr) {
            return service->GetApi() == provider.get();
        }
        ProcessArgs(args);
        auto *sceneService = Interface<ILauncherSceneSource>::Get();
        if (!providerOnly && sceneService->GetApi() != nullptr) {
            LOG_E(TAG, "Launcher scene source is already registered");
            return false;
        }
        if (!shaderUsagePath.empty()) {
            ShaderUsageStore::Clear();
            ShaderUsageStore::SetEnabled(true);
        }

        if (!provider)
            provider = CreateRendererProvider();
        auto       *skeletonService = Interface<IAnimationSkeletonSource>::Get();
        const auto *system          = Interface<ISystemNotify>::Get()->GetApi();
        auto       *modules         = system ? system->GetModuleManager() : ModuleManager::Current();
        if (skeletonService->GetApi()) {
            provider.reset();
            return false;
        }
        skeletonSource = CreateAnimationSkeletonSource(modules ? modules->RetainModule("Aurora.Render") : nullptr);
        if (!skeletonSource) {
            provider.reset();
            return false;
        }
        skeletonService->Register(*skeletonSource);
        if (providerOnly) {
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
        service->Register(*provider);
        sceneThread = std::this_thread::get_id();
        sceneService->Register(*this);
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
        if (const auto error = scene.Create(*pending, {0, sceneGeneration}, {Name("main")})) {
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
                const auto active = sceneSession;
                if (active)
                    active->session->OnEvent(event);
                if (shutdownRequested)
                    return;
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
        if (closed || outputState != OutputState::ABSENT || !desiredSurface || !scene.GetSceneId().IsValid())
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

    bool AuroraModule::UpdateScene(FramePacket &packet)
    {
        if (resetScene && scene.GetSceneId().IsValid()) {
            if (const auto error = scene.Destroy(packet)) {
                Fail(*error);
                return false;
            }
        }
        if (!resetScene || scene.IsRetiring())
            return !scene.IsRetiring();
        if (sceneGeneration == UINT32_MAX) {
            Fail({RendererErrorCode::OUT_OF_MEMORY, "Launcher scene generation exhausted"});
            return false;
        }
        if (const auto error = scene.Create(packet, {0, ++sceneGeneration}, {Name("main")})) {
            Fail(*error);
            return false;
        }
        resetScene            = false;
        viewDescription.scene = scene.GetSceneId();
        if (viewCreated && outputState == OutputState::LIVE)
            packet.commands.push_back(UpdateView{view, viewDescription, 0});
        return true;
    }

    void AuroraModule::Pump(float delta, bool produceScene)
    {
        if (pumping || !host.Get())
            return;
        pumping = true;
        struct PumpGuard {
            bool         &active;
            bool         &shutdown;
            AuroraModule &owner;
            ~PumpGuard()
            {
                active = false;
                if (shutdown) {
                    shutdown = false;
                    owner.Shutdown();
                }
            }
        } guard{pumping, shutdownRequested, *this};
        PollEvents();
        if (shutdownRequested || !started || host.Get()->GetState() != RendererState::RUNNING)
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
        if (!closed) {
            if (const auto error = UpdateDefaultSceneSession()) {
                Fail(*error);
                return;
            }
        }
        pending               = std::make_unique<FramePacket>();
        pending->sequence     = ++sequence;
        pending->deltaSeconds = delta;
        const bool sceneReady = !closed && UpdateScene(*pending);
        if (!pending)
            return;
        UpdateOutput(*pending);
        const auto active = sceneSession;
        if (sceneReady && produceScene && active) {
            if (const auto error = active->session->Append(*pending, {scene.GetSceneId(), view, viewport, extent})) {
                Fail(*error);
                return;
            }
            if (shutdownRequested)
                return;
        }
        // Keep the owned main view's CPU settings while its output is absent.
        std::erase_if(pending->commands, [&](RenderCommand &command) {
            auto *update = std::get_if<UpdateView>(&command);
            if (!update || update->id != view)
                return false;
            viewDescription        = update->desc;
            viewDescription.scene  = scene.GetSceneId();
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
            if (scene.GetSceneId().IsValid()) {
                if (const auto error = scene.Destroy(*pending)) {
                    Fail(*error);
                    return;
                }
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
        auto *sceneService = Interface<ILauncherSceneSource>::Get();
        if (sceneService->GetApi() == this) {
            sceneService->UnRegister();
        }
        sceneSession.reset();
        if (pumping) {
            shutdownRequested = true;
            return;
        }
        auto *skeletonService = Interface<IAnimationSkeletonSource>::Get();
        if (skeletonSource && skeletonService->GetApi() == skeletonSource.get()) {
            skeletonService->UnRegister();
        }
        skeletonSource.reset();
        const bool active = provider || host.Get();
        Event<IWindowEvent>::DisConnect(this);
        pending.reset();
        host.Shutdown();
        activeSurfaceLease.reset();
        desiredSurfaceLease.reset();
        scene.OnRendererStopped();
        resetScene = false;
        if (window)
            window->SetCloseDeferred(false);
        window = nullptr;
        if (provider) {
            auto *service = Interface<IRendererProvider>::Get();
            if (service->GetApi() == provider.get())
                service->UnRegister();
            provider.reset();
        }
        started = closed = retiring = false;
        activeSurface = desiredSurface = 0;
        outputState                    = OutputState::ABSENT;
        viewport                       = {0, 1};
        viewCreated = replaceOutput = false;
        viewRevision                = 0;
        viewDescription             = {};
        sequence = revision = 0;
        if (!shaderUsagePath.empty()) {
            if (!ShaderUsageStore::Flush(shaderUsagePath)) {
                LOG_E(TAG, "Failed to write shader usage: %s", shaderUsagePath.c_str());
            }
            ShaderUsageStore::SetEnabled(false);
            ShaderUsageStore::Clear();
            shaderUsagePath.clear();
        }
        if (active)
            LOG_I(TAG, "Renderer host stopped");
    }

} // namespace sky::aurora
