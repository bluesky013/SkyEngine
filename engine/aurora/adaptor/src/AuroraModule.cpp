//
// AuroraRender module implementation: creates the Aurora device, a window
// swapchain viewport and drives a minimal clear/present frame loop.
//

#include <aurora/adaptor/AuroraModule.h>

#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/rdg/ClientViewport.h>
#include <aurora/rdg/RenderDeviceExclusive.h>
#include <aurora/rhi/CommandBuffer.h>
#include <aurora/rhi/Device.h>
#include <aurora/rhi/Encoder.h>
#include <aurora/rhi/Image.h>
#include <aurora/rhi/Instance.h>
#include <aurora/rhi/Queue.h>
#include <aurora/rhi/Semaphore.h>
#include <aurora/rhi/SubmitInfo.h>
#include <aurora/rhi/SwapChain.h>

#include <core/cmdline/CmdParser.h>
#include <core/logger/Logger.h>
#include <core/name/Name.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/platform/PlatformBase.h>
#include <framework/serialization/SerializationContext.h>

static const char *TAG = "AuroraModule";

namespace sky::aurora {

    AuroraModule::~AuroraModule() = default;

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
        CmdOptions options("Aurora Render Module", "Aurora Render Module");
        options.allow_unrecognised_options();
        options.add_options()("r,rhi", "RHI Type", CmdValue<std::string>());

        if (args.args.empty()) {
            return;
        }

        auto result = options.parse(static_cast<int32_t>(args.args.size()), args.args.data());
        if (result.count("rhi") != 0u) {
            mApi = ParseApi(result["rhi"].as<std::string>());
        }
    }

    bool AuroraModule::Init(const StartArguments &args)
    {
        ProcessArgs(args);

        // Register aurora types / asset handlers / components with the
        // framework before the first tick (idempotent).
        if (auto *context = SerializationContext::Get()) {
            AuroraReflection(context);
        }

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

    void AuroraModule::Start()
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

    void AuroraModule::Tick(float /*delta*/)
    {
        if (mDevice == nullptr || mFrameContext == nullptr || mCommandBuffer == nullptr || mViewport == nullptr) {
            return;
        }

        mFrameContext->BeginFrame();

        if (!mViewport->Begin() || !mViewport->Acquire()) {
            mFrameContext->EndFrame();
            return;
        }

        Image       *backbuffer = mViewport->GetBackbuffer();
        const Extent2D extent   = mViewport->GetExtent();
        if (backbuffer == nullptr || extent.width == 0 || extent.height == 0) {
            mFrameContext->EndFrame();
            return;
        }

        mCommandBuffer->Begin();
        {
            BarrierInfo   barrier{};
            barrier.srcStage    = PipelineStageBit::TOP;
            barrier.dstStage    = PipelineStageBit::COLOR_OUTPUT;
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
            auto encoder = mCommandBuffer->CreateGraphicsEncoder();
            RenderingInfo info  = {};
            info.renderArea     = {{0, 0}, extent};
            info.numColors      = 1;
            info.colors[0].image      = backbuffer;
            info.colors[0].loadOp     = LoadOp::CLEAR;
            info.colors[0].storeOp    = StoreOp::STORE;
            info.colors[0].clearValue = ClearValue(0.08f, 0.10f, 0.14f, 1.0f);
            encoder->BeginRendering(info);
            encoder->EndRendering();
        }
        {
            BarrierInfo   barrier{};
            barrier.srcStage    = PipelineStageBit::COLOR_OUTPUT;
            barrier.dstStage    = PipelineStageBit::BOTTOM;
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

    void AuroraModule::Shutdown()
    {
        mViewport.reset();
        mCommandBuffer = nullptr;
        mCommandPool.reset();
        mFrameContext.reset();
        mDevice = nullptr;
        // Instance teardown (device + backend dll) is owned by the Instance singleton.
    }

} // namespace sky::aurora
