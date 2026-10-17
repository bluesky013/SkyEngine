//
// Aurora launcher producer and the unmigrated platform host path.
//

#include <aurora/adaptor/AuroraModule.h>

#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/renderer/RendererProvider.h>
#if !defined(_WIN32)
    #include <aurora/rdg/ClientViewport.h>
    #include <aurora/rdg/DeviceFrameContext.h>
    #include <aurora/rhi/CommandBuffer.h>
    #include <aurora/rhi/Device.h>
    #include <aurora/rhi/Encoder.h>
    #include <aurora/rhi/Image.h>
    #include <aurora/rhi/Instance.h>
    #include <aurora/rhi/Queue.h>
    #include <aurora/rhi/Semaphore.h>
    #include <aurora/rhi/SubmitInfo.h>
    #include <aurora/rhi/SwapChain.h>
#endif

#include <array>
#include <core/cmdline/CmdParser.h>
#include <core/logger/Logger.h>
#include <core/name/Name.h>
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
#if !defined(_WIN32)
        legacy = true;
        if (!InitLegacy()) {
            Shutdown();
            return false;
        }
#else
        {
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
        }
#endif
        AuroraReflection(SerializationContext::Get());
        service->Register(*provider);
        return true;
    }

#if !defined(_WIN32)
    bool AuroraModule::InitLegacy()
    {
        Instance::Descriptor desc = {};
        desc.appName              = "SkyGame";
        desc.engineName           = "SkyEngine";
    #if defined(_DEBUG)
        desc.enableDebugLayer = true;
    #else
        desc.enableDebugLayer = false;
    #endif
        desc.api = mApi;

        Instance::Get()->Init(desc);
        mDevice = Instance::Get()->GetDevice();
        if (mDevice == nullptr) {
            LOG_E(TAG, "aurora device init failed");
            return false;
        }

        DeviceFrameContextInitInfo info{};
        info.inflightNum = 1; // single in-flight frame: the command buffer is reused safely
        info.parallelNum = 1;
        mFrameContext.reset(mDevice->CreateFrameContext(info));
        if (!mFrameContext) {
            LOG_E(TAG, "aurora frame context creation failed");
            return false;
        }

        mCommandPool.reset(mDevice->CreateCommandPool(QueueType::GRAPHICS));
        if (!mCommandPool || !mCommandPool->Init()) {
            LOG_E(TAG, "aurora command pool init failed");
            return false;
        }
        mCommandBuffer = mCommandPool->Allocate();
        if (mCommandBuffer == nullptr) {
            LOG_E(TAG, "aurora command buffer allocation failed");
            return false;
        }
        return true;
    }

    void AuroraModule::StartLegacy()
    {
        if (mDevice == nullptr) {
            return;
        }

        void *window = nullptr;
        if (auto *system = Interface<ISystemNotify>::Get()->GetApi()) {
            window = system->GetMainWindowHandle();
        }
        if (window == nullptr) {
            window = Platform::Get()->GetMainWinHandle();
        }
        if (window == nullptr) {
            LOG_E(TAG, "no main window handle; skipping viewport");
            return;
        }

        SwapChain::Descriptor scDesc = {};
        scDesc.window                = window;
        scDesc.width                 = 1280;
        scDesc.height                = 720;
        scDesc.preferredFormat       = PixelFormat::BGRA8_UNORM;
        scDesc.preferredMode         = PresentMode::IMMEDIATE;

        mViewport = std::make_unique<ClientViewport>(Name("main"));
        if (!mViewport->Init(mDevice, scDesc)) {
            LOG_E(TAG, "client viewport init failed");
            mViewport.reset();
        }
    }

    void AuroraModule::TickLegacy()
    {
        if (mDevice == nullptr || mFrameContext == nullptr || mCommandBuffer == nullptr || mViewport == nullptr) {
            return;
        }

        mFrameContext->BeginFrame();

        if (!mViewport->Begin()) {
            mFrameContext->EndFrame();
            return;
        }

        // Check the surface size before acquiring so a minimized/zero-size window
        // never acquires an image it cannot present (which would stall the loop).
        const Extent2D extent = mViewport->GetExtent();
        if (extent.width == 0 || extent.height == 0) {
            mFrameContext->EndFrame();
            return;
        }
        if (!mViewport->Acquire()) {
            mFrameContext->EndFrame();
            return;
        }

        Image *backbuffer = mViewport->GetBackbuffer();
        if (backbuffer == nullptr) {
            mViewport->Release(); // acquired but unusable; present to keep the queue moving
            mFrameContext->EndFrame();
            return;
        }

        mCommandBuffer->Begin();
        {
            BarrierInfo barrier{};
            barrier.srcStage = PipelineStageBit::TOP;
            barrier.dstStage = PipelineStageBit::COLOR_OUTPUT;
            ImageBarrierInfo ib{};
            ib.image     = backbuffer;
            ib.subRange  = ImageSubRange{};
            ib.srcAccess = AccessFlagBit::NONE;
            ib.dstAccess = AccessFlagBit::RTV;
            ib.oldLayout = ImageLayout::UNDEFINED;
            ib.newLayout = ImageLayout::COLOR_ATTACHMENT;
            barrier.imageBarriers.push_back(ib);
            mCommandBuffer->PipelineBarrier(barrier);
        }
        {
            auto          encoder     = mCommandBuffer->CreateGraphicsEncoder();
            RenderingInfo info        = {};
            info.renderArea           = {{0, 0}, extent};
            info.numColors            = 1;
            info.colors[0].image      = backbuffer;
            info.colors[0].loadOp     = LoadOp::CLEAR;
            info.colors[0].storeOp    = StoreOp::STORE;
            info.colors[0].clearValue = ClearValue(0.08f, 0.10f, 0.14f, 1.0f);
            encoder->BeginRendering(info);
            encoder->EndRendering();
        }
        {
            BarrierInfo barrier{};
            barrier.srcStage = PipelineStageBit::COLOR_OUTPUT;
            barrier.dstStage = PipelineStageBit::BOTTOM;
            ImageBarrierInfo ib{};
            ib.image     = backbuffer;
            ib.subRange  = ImageSubRange{};
            ib.srcAccess = AccessFlagBit::RTV;
            ib.dstAccess = AccessFlagBit::PRESENT;
            ib.oldLayout = ImageLayout::COLOR_ATTACHMENT;
            ib.newLayout = ImageLayout::PRESENT;
            barrier.imageBarriers.push_back(ib);
            mCommandBuffer->PipelineBarrier(barrier);
        }
        mCommandBuffer->End();

        SubmitInfo submit = {};
        submit.commandBuffers.push_back(mCommandBuffer);

        SemaphoreSubmitInfo wait = {};
        wait.semaphore           = mViewport->GetAcquireSemaphore();
        wait.stageMask           = PipelineStageBit::COLOR_OUTPUT;
        submit.waitSemaphores.push_back(wait);

        SemaphoreSubmitInfo signal = {};
        signal.semaphore           = mViewport->GetRenderDoneSemaphore();
        signal.stageMask           = PipelineStageBit::BOTTOM;
        submit.signalSemaphores.push_back(signal);

        submit.fence = mFrameContext->GetFrameFence();
        mDevice->GetQueue(QueueType::GRAPHICS)->Submit(submit);

        mViewport->Release();
        mFrameContext->EndFrame();
    }

#endif

    void AuroraModule::Start()
    {
#if !defined(_WIN32)
        if (legacy) {
            StartLegacy();
            return;
        }
#endif
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
        pending           = std::make_unique<FramePacket>();
        pending->sequence = ++sequence;
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
#if !defined(_WIN32)
        if (legacy) {
            TickLegacy();
            return;
        }
#endif
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
#if !defined(_WIN32)
        if (mDevice)
            mDevice->WaitIdle();
        mViewport.reset();
        mCommandBuffer = nullptr;
        mCommandPool.reset();
        mFrameContext.reset();
        mDevice = nullptr;
        legacy  = false;
#endif
        sceneSource = {};
        started = closed = retiring = false;
        sequence = revision = 0;
        if (active)
            LOG_I(TAG, "Renderer host stopped");
    }

} // namespace sky::aurora
