//
// Created on 2026/10/16.
//

#include "GpuTestSupport.h"
#include "rdg/D3D12FrameCompiler.h"
#include "rdg/VulkanFrameCompiler.h"

#include "graph/FrameGraphBuilder.h"
#include <D3D12Device.h>
#include <aurora/rdg/DeviceFrameContext.h>
#include <common/FrameTransientPool.h>
#include <d3d12sdklayers.h>

namespace {
    class BufferAliasBarrierProbe {
    public:
        explicit BufferAliasBarrierProbe(VulkanDevice &device)
            : fn(const_cast<VulkanDeviceFunctions &>(device.GetDeviceFn())), saved(fn.vkCmdPipelineBarrier2)
        {
            active                   = this;
            fn.vkCmdPipelineBarrier2 = Barrier;
        }
        ~BufferAliasBarrierProbe()
        {
            fn.vkCmdPipelineBarrier2 = saved;
            active                   = nullptr;
        }
        uint32_t writeDependencies = 0;

    private:
        static void VKAPI_PTR Barrier(VkCommandBuffer command, const VkDependencyInfo *info)
        {
            for (uint32_t i = 0; i < info->bufferMemoryBarrierCount; ++i) {
                const auto &barrier = info->pBufferMemoryBarriers[i];
                if ((barrier.srcAccessMask & VK_ACCESS_2_TRANSFER_WRITE_BIT) && (barrier.dstAccessMask & VK_ACCESS_2_TRANSFER_WRITE_BIT))
                    ++active->writeDependencies;
            }
            active->saved(command, info);
        }
        inline static BufferAliasBarrierProbe *active = nullptr;
        VulkanDeviceFunctions                 &fn;
        PFN_vkCmdPipelineBarrier2              saved;
    };
    class FrameTransient : public testing::TestWithParam<uint32_t> {
    protected:
        void SetUp() override
        {
            Instance::Get()->Init({"FrameTransientTest", "SkyEngine", true, GetParam() == 2 ? API::DX12 : API::VULKAN});
            device = Instance::Get()->GetDevice();
            ASSERT_TRUE(device);
            if (GetParam() == 2) {
                ASSERT_TRUE(SUCCEEDED(static_cast<D3D12Device *>(device)->GetNativeHandle()->QueryInterface(IID_PPV_ARGS(&debug))));
                debug->ClearStoredMessages();
            }
            info.deviceEpoch    = 149;
            info.parallelNum    = 3;
            info.vulkanCompiler = GetParam() == 1 ? VulkanFrameGraphMode::RENDER_PASS : VulkanFrameGraphMode::DYNAMIC_RENDERING;
            context.reset(device->CreateFrameContext(info));
            ASSERT_TRUE(context);
        }
        void TearDown() override
        {
            context.reset();
            if (debug) {
                for (UINT64 i = 0; i < debug->GetNumStoredMessages(); ++i) {
                    SIZE_T size = 0;
                    debug->GetMessage(i, nullptr, &size);
                    std::vector<char> bytes(size);
                    auto             *message = reinterpret_cast<D3D12_MESSAGE *>(bytes.data());
                    if (SUCCEEDED(debug->GetMessage(i, message, &size)))
                        EXPECT_GT(message->Severity, D3D12_MESSAGE_SEVERITY_ERROR) << message->pDescription;
                }
            }
            debug.Reset();
            Instance::Destroy();
        }
        FrameTransientPool &Pool()
        {
            return GetParam() == 2 ? static_cast<D3D12Device *>(device)->GetTransientPool() : static_cast<VulkanDevice *>(device)->GetTransientPool();
        }
        Buffer::Descriptor BufferDesc()
        {
            Buffer::Descriptor desc;
            desc.size  = 256;
            desc.usage = BufferUsageFlagBit::TRANSFER_SRC | BufferUsageFlagBit::TRANSFER_DST;
            return desc;
        }
        Image::Descriptor ImageDesc()
        {
            Image::Descriptor desc;
            desc.format = PixelFormat::RGBA8_UNORM;
            desc.extent = {8, 8, 1};
            desc.usage  = ImageUsageFlagBit::TRANSFER_SRC | ImageUsageFlagBit::TRANSFER_DST;
            return desc;
        }
        BufferPtr Host(bool upload, uint64_t size = 256)
        {
            auto desc   = BufferDesc();
            desc.size   = size;
            desc.memory = upload || GetParam() != 2 ? MemoryType::CPU_ONLY : MemoryType::GPU_TO_CPU;
            desc.usage  = upload ? BufferUsageFlagBit::TRANSFER_SRC : BufferUsageFlagBit::TRANSFER_DST;
            return BufferPtr(device->CreateBuffer(desc));
        }
        GraphBufferHandle Import(FrameGraphBuilder &builder, const BufferPtr &buffer, bool upload)
        {
            const auto state = upload ? CopyBufferState::HOST_WRITE : CopyBufferState::HOST_READ;
            return std::get<GraphBufferHandle>(builder.Import(buffer, state, state, upload));
        }
        std::shared_ptr<const FrozenFrameGraph> Freeze(FrameGraphBuilder &builder)
        {
            return std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
        }
        std::shared_ptr<const FrozenFrameGraph> Graph(const BufferPtr &source, const BufferPtr &output, bool aliases = false)
        {
            FrameGraphBuilder builder;
            const auto        input  = Import(builder, source, true);
            const auto        result = Import(builder, output, false);
            const auto        buffer = std::get<GraphBufferHandle>(builder.CreateTransient(BufferDesc()));
            auto              desc   = ImageDesc();
            desc.mipLevels           = 2;
            desc.arrayLayers         = 2;
            const auto image         = std::get<GraphImageHandle>(builder.CreateTransient(desc));
            builder.Copy({input, buffer, 0, 0, 256});
            BufferImageCopy region;
            region.imageExtent = {4, 4, 1};
            region.subRange    = {1, 1, 1, AspectFlagBit::COLOR_BIT};
            builder.Copy({buffer, image, region});
            builder.Copy({result, image, region, ImageCopyDirection::IMAGE_TO_BUFFER});
            if (aliases) {
                const auto nextBuffer = std::get<GraphBufferHandle>(builder.CreateTransient(BufferDesc()));
                const auto nextImage  = std::get<GraphImageHandle>(builder.CreateTransient(desc));
                builder.Copy({input, nextBuffer, 64, 0, 64});
                builder.Copy({nextBuffer, nextImage, region});
                region.bufferOffset = 64;
                builder.Copy({result, nextImage, region, ImageCopyDirection::IMAGE_TO_BUFFER});
            }
            builder.Export({result, 0, aliases ? 128u : 64u});
            return Freeze(builder);
        }
        const FrameResourceBindings &Bindings(const BackendCompiledFrame &plan)
        {
            return GetParam() == 2 ? static_cast<const D3D12CompiledFrame &>(plan).resources
                                   : static_cast<const VulkanCompiledFrame &>(plan).resources;
        }
        ShaderPtr ShaderOf(bool green)
        {
            const auto vs = CompileGpuFunction(*device, kGpuFullscreenSource, ShaderStageFlagBit::VS, "VSMain");
            const auto fs = CompileGpuFunction(*device, kGpuFullscreenSource, ShaderStageFlagBit::FS, green ? "Green" : "Red");
            if (!vs || !fs)
                return {};
            Shader::Descriptor descriptor{};
            descriptor.vs = vs.Get();
            descriptor.ps = fs.Get();
            return ShaderPtr(device->CreateShader(descriptor));
        }
        DeviceFrameLease Begin(DeviceFrameContext &ctx)
        {
            auto result = ctx.TryBeginSlot();
            EXPECT_TRUE(std::holds_alternative<DeviceFrameLease>(result));
            auto *lease = std::get_if<DeviceFrameLease>(&result);
            return lease ? *lease : DeviceFrameLease{};
        }
        std::unique_ptr<BackendCompiledFrame>
        Compile(DeviceFrameContext &ctx, const DeviceFrameLease &lease, const std::shared_ptr<const FrozenFrameGraph> &graph)
        {
            auto  result = ctx.Compile(lease, graph);
            auto *plan   = std::get_if<std::unique_ptr<BackendCompiledFrame>>(&result);
            EXPECT_TRUE(plan);
            return plan ? std::move(*plan) : nullptr;
        }
        FrameCollectResult Collect(EncodingTicket ticket)
        {
            const auto deadline = std::chrono::steady_clock::now() + 5s;
            auto       result   = context->TryCollect(ticket);
            while (std::holds_alternative<FramePending>(result) && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(1ms);
                result = context->TryCollect(ticket);
            }
            return result;
        }
        std::unique_ptr<RecordedFrame> Record(std::unique_ptr<BackendCompiledFrame> plan)
        {
            auto  dispatched = context->Dispatch(plan);
            auto *ticket     = std::get_if<EncodingTicket>(&dispatched);
            EXPECT_TRUE(ticket);
            if (!ticket)
                return {};
            auto  collected = Collect(*ticket);
            auto *recording = std::get_if<std::unique_ptr<RecordedFrame>>(&collected);
            EXPECT_TRUE(recording);
            return recording ? std::move(*recording) : nullptr;
        }
        void Complete()
        {
            std::array<FrameCompletion, 1> completed;
            const auto                     deadline = std::chrono::steady_clock::now() + 5s;
            while (std::chrono::steady_clock::now() < deadline) {
                if (context->PollCompleted(completed)) {
                    EXPECT_EQ(completed[0].status, FrameCompletionStatus::COMPLETE);
                    return;
                }
                std::this_thread::sleep_for(1ms);
            }
            FAIL() << "Completion timed out";
        }
        Device                             *device = nullptr;
        DeviceFrameContextInitInfo          info;
        std::unique_ptr<DeviceFrameContext> context;
        ComPtr<ID3D12InfoQueue>             debug;
    };
} // namespace

TEST_P(FrameTransient, CopyContentsAndBackingReuseAfterRetirement)
{
    auto source = Host(true), output = Host(false);
    ASSERT_TRUE(source && output);
    const auto graph = Graph(source, output);
    EXPECT_FALSE(graph->GetData().buffers[2].buffer);
    EXPECT_FALSE(graph->GetData().images[0].image);
    for (uint32_t iteration = 0; iteration < 3; ++iteration) {
        auto *bytes = source->Map();
        ASSERT_TRUE(bytes);
        std::memset(bytes, 21 + iteration, 256);
        source->UnMap();
        auto plan = Compile(*context, Begin(*context), graph);
        ASSERT_TRUE(plan);
        EXPECT_EQ(Pool().GetStats().borrowed, 2u);
        auto recording = Record(std::move(plan));
        ASSERT_TRUE(recording);
        ASSERT_TRUE(recording->GetGraphReceipt());
        EXPECT_EQ(recording->GetGraphReceipt()->graph.get(), graph.get());
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
        EXPECT_EQ(Pool().GetStats().borrowed, 2u);
        Complete();
        EXPECT_EQ(Pool().GetStats().borrowed, 0u);
        auto *read = output->Map();
        ASSERT_TRUE(read);
        for (uint32_t i = 0; i < 64; ++i)
            EXPECT_EQ(read[i], 21 + iteration);
        output->UnMap();
    }
    EXPECT_EQ(Pool().GetStats().allocations, 2u);
    EXPECT_EQ(Pool().GetStats().reuses, 4u);
}

TEST_P(FrameTransient, DevicePoolSharesRetiredObjectsButIsolatesBorrowersAndEpochs)
{
    auto       graph      = Graph(Host(true), Host(false), true);
    const auto firstLease = Begin(*context);
    auto       first      = Compile(*context, firstLease, graph);
    ASSERT_TRUE(first);
    std::unique_ptr<DeviceFrameContext> other(device->CreateFrameContext(info));
    ASSERT_TRUE(other);
    const auto otherLease = Begin(*other);
    auto       second     = Compile(*other, otherLease, graph);
    ASSERT_TRUE(second);
    EXPECT_EQ(Pool().GetStats().allocations, 4u);
    EXPECT_EQ(Pool().GetStats().borrowed, 4u);
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->DiscardUnsubmitted(firstLease)));
    first.reset();
    EXPECT_EQ(Pool().GetStats().borrowed, 2u);
    auto reused = Compile(*context, firstLease, graph);
    ASSERT_TRUE(reused);
    EXPECT_EQ(Pool().GetStats().reuses, 2u);
    reused.reset();
    second.reset();
    other.reset();
    ++info.deviceEpoch;
    other.reset(device->CreateFrameContext(info));
    auto newEpoch = Compile(*other, Begin(*other), graph);
    ASSERT_TRUE(newEpoch);
    EXPECT_EQ(Pool().GetStats().allocations, 6u);
    EXPECT_EQ(Pool().GetStats().reuses, 2u);
    newEpoch.reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(firstLease)));
}

TEST_P(FrameTransient, RecordingAndSubmissionKeepPinsUntilPoll)
{
    auto graph     = Graph(Host(true), Host(false), true);
    auto recording = Record(Compile(*context, Begin(*context), graph));
    ASSERT_TRUE(recording);
    std::unique_ptr<DeviceFrameContext> other(device->CreateFrameContext(info));
    ASSERT_TRUE(other);
    EXPECT_TRUE(std::holds_alternative<SubmissionRejected>(other->Submit(recording)));
    EXPECT_TRUE(recording);
    EXPECT_EQ(Pool().GetStats().borrowed, 2u);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    device->WaitIdle();
    auto another = Compile(*other, Begin(*other), graph);
    ASSERT_TRUE(another);
    EXPECT_EQ(Pool().GetStats().allocations, 4u);
    EXPECT_EQ(Pool().GetStats().borrowed, 4u);
    Complete();
    EXPECT_EQ(Pool().GetStats().borrowed, 2u);
}

TEST_P(FrameTransient, DeadDeclarationsAndUndefinedContentsDoNotAllocate)
{
    FrameGraphBuilder builder;
    const auto        buffer = std::get<GraphBufferHandle>(builder.CreateTransient(BufferDesc()));
    const auto        other  = std::get<GraphBufferHandle>(builder.CreateTransient(BufferDesc()));
    builder.Copy({buffer, other, 0, 0, 256});
    builder.CreateTransient(ImageDesc());
    auto       graph = Freeze(builder);
    const auto lease = Begin(*context);
    EXPECT_TRUE(std::holds_alternative<FrameNoWork>(context->Compile(lease, graph)));
    EXPECT_EQ(Pool().GetStats().allocations, 0u);
    auto data = graph->GetData();
    data.exports.push_back({other, 0, 256});
    auto invalid = std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->Compile(lease, invalid)));
    EXPECT_EQ(Pool().GetStats().allocations, 0u);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(FrameTransient, CompileFailureReturnsPinsAndCancelReleasesRecording)
{
    auto graph                       = Graph(Host(true), Host(false), true);
    auto data                        = graph->GetData();
    data.buffers[2].transient->usage = BufferUsageFlagBit::TRANSFER_DST;
    auto       invalid               = std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data));
    const auto lease                 = Begin(*context);
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->Compile(lease, invalid)));
    EXPECT_EQ(Pool().GetStats().borrowed, 0u);
    auto plan = Compile(*context, lease, graph);
    ASSERT_TRUE(plan);
    auto dispatch = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatch));
    const auto ticket = std::get<EncodingTicket>(dispatch);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->Cancel(ticket)));
    auto collected = Collect(ticket);
    EXPECT_TRUE(std::holds_alternative<EncodingCancelled>(collected));
    collected = FramePending{};
    EXPECT_EQ(Pool().GetStats().borrowed, 0u);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(FrameTransient, CacheBudgetNeverEvictsBorrowedObjectsAndFailureRollsBackPins)
{
    auto       graph    = Graph(Host(true), Host(false));
    const auto analyzed = AnalyzeFrameGraph(*graph);
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analyzed));
    const auto        &analysis = std::get<FrameGraphAnalysis>(analyzed);
    FrameTransientPool pool(*device, {2, 4096});
    auto               first = pool.Acquire(graph->GetData(), analysis, 1);
    ASSERT_TRUE(std::holds_alternative<FrameResourceBindings>(first));
    auto failed = pool.Acquire(graph->GetData(), analysis, 1);
    ASSERT_TRUE(std::holds_alternative<FrameError>(failed));
    EXPECT_EQ(std::get<FrameError>(failed).code, FrameErrorCode::OUT_OF_MEMORY);
    EXPECT_EQ(pool.GetStats().borrowed, 2u);
    first = FrameError{FrameErrorCode::INVALID_STATE};
    EXPECT_EQ(pool.GetStats().borrowed, 0u);
    auto changed                       = graph->GetData();
    changed.buffers[2].transient->size = changed.buffers[2].size = 512;
    auto replaced                                                = pool.Acquire(changed, analysis, 2);
    ASSERT_TRUE(std::holds_alternative<FrameResourceBindings>(replaced));
    EXPECT_EQ(pool.GetStats().evictions, 2u);
    replaced = FrameError{FrameErrorCode::INVALID_STATE};
    FrameTransientPool tiny(*device, {2, 256});
    EXPECT_TRUE(std::holds_alternative<FrameError>(tiny.Acquire(graph->GetData(), analysis, 1)));
    EXPECT_EQ(tiny.GetStats().borrowed, 0u);
    EXPECT_EQ(tiny.GetStats().allocations, 1u);
}

TEST_P(FrameTransient, TransientColorAndDepthClearSurviveReuse)
{
    auto              output = Host(false);
    FrameGraphBuilder builder;
    const auto        result = Import(builder, output, false);
    auto              desc   = ImageDesc();
    desc.usage |= ImageUsageFlagBit::RENDER_TARGET;
    const auto color      = std::get<GraphImageHandle>(builder.CreateTransient(desc));
    desc.format           = PixelFormat::D32;
    desc.usage            = ImageUsageFlagBit::DEPTH_STENCIL;
    const auto      depth = std::get<GraphImageHandle>(builder.CreateTransient(desc));
    FrameRasterPass raster;
    raster.color = color;
    raster.clear = ClearValue{1.f, 0.f, 0.f, 1.f};
    raster.depth = RasterDepthAttachment{depth};
    builder.Raster(raster);
    BufferImageCopy region;
    region.imageExtent = {8, 8, 1};
    region.subRange    = {0, 0, 1, AspectFlagBit::COLOR_BIT};
    builder.Copy({result, color, region, ImageCopyDirection::IMAGE_TO_BUFFER});
    builder.Export({result, 0, 256});
    auto graph = Freeze(builder);
    for (uint32_t i = 0; i < 2; ++i) {
        auto recording = Record(Compile(*context, Begin(*context), graph));
        ASSERT_TRUE(recording);
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
        Complete();
        auto *bytes = output->Map();
        ASSERT_TRUE(bytes);
        for (uint32_t p = 0; p < 256; ++p)
            EXPECT_EQ(bytes[p], p % 4 == 0 || p % 4 == 3 ? 255 : 0);
        output->UnMap();
    }
    EXPECT_EQ(Pool().GetStats().allocations, 2u);
    EXPECT_EQ(Pool().GetStats().reuses, 2u);
}

TEST_P(FrameTransient, UncertainSubmitAndQueryFailureQuarantinePinsUntilTeardown)
{
    auto      *vk      = GetParam() == 2 ? nullptr : &const_cast<VulkanDeviceFunctions &>(static_cast<VulkanDevice *>(device)->GetDeviceFn());
    auto      *dx      = GetParam() == 2 ? &const_cast<D3D12Functions &>(static_cast<D3D12Device *>(device)->GetFunctions()) : nullptr;
    const auto savedVk = vk ? *vk : VulkanDeviceFunctions{};
    const auto savedDx = dx ? *dx : D3D12Functions{};
    Finally    cleanup{[&] {
        if (vk)
            *vk = savedVk;
        if (dx)
            *dx = savedDx;
        context.reset();
    }};
    for (bool uncertain : {false, true}) {
        context.reset(device->CreateFrameContext(info));
        ASSERT_TRUE(context);
        auto recording = Record(Compile(*context, Begin(*context), Graph(Host(true), Host(false), true)));
        ASSERT_TRUE(recording);
        if (uncertain) {
            if (vk)
                vk->vkQueueSubmit2 = [](VkQueue, uint32_t, const VkSubmitInfo2 *, VkFence) -> VkResult { return VK_ERROR_UNKNOWN; };
            if (dx)
                dx->signal = [](ID3D12CommandQueue *, ID3D12Fence *, UINT64) -> HRESULT { return E_FAIL; };
        }
        const auto submitted = context->Submit(recording);
        EXPECT_EQ(std::holds_alternative<SubmissionIndeterminate>(submitted), uncertain);
        EXPECT_EQ(std::holds_alternative<SubmissionAccepted>(submitted), !uncertain);
        ASSERT_FALSE(recording);
        if (vk) {
            vk->vkQueueSubmit2   = savedVk.vkQueueSubmit2;
            vk->vkGetFenceStatus = [](VkDevice, VkFence) -> VkResult { return VK_NOT_READY; };
        }
        if (dx) {
            dx->signal    = savedDx.signal;
            dx->completed = [](ID3D12Fence *) -> UINT64 { return 0; };
        }
        std::array<FrameCompletion, 1> completed;
        EXPECT_EQ(context->PollCompleted(completed), 0u);
        EXPECT_EQ(Pool().GetStats().borrowed, 2u);
        if (vk)
            vk->vkGetFenceStatus = [](VkDevice, VkFence) -> VkResult { return VK_ERROR_DEVICE_LOST; };
        if (dx)
            dx->completed = [](ID3D12Fence *) -> UINT64 { return UINT64_MAX; };
        ASSERT_EQ(context->PollCompleted(completed), 1u);
        EXPECT_EQ(completed[0].status, FrameCompletionStatus::DEVICE_LOST);
        EXPECT_EQ(context->PollCompleted(completed), 0u);
        EXPECT_EQ(Pool().GetStats().borrowed, 2u);
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryBeginSlot()));
        if (vk)
            *vk = savedVk;
        if (dx)
            *dx = savedDx;
        context.reset();
        EXPECT_EQ(Pool().GetStats().borrowed, 0u);
    }
}

TEST_P(FrameTransient, MalformedDeclarationsFailBeforeAnyAllocation)
{
    auto       graph = Graph(Host(true), Host(false));
    const auto lease = Begin(*context);
    for (uint32_t test = 0; test < 13; ++test) {
        auto  data   = graph->GetData();
        auto &buffer = data.buffers[2];
        auto &image  = data.images[0];
        switch (test) {
        case 0: buffer.initialized = true; break;
        case 1: buffer.initial = CopyBufferState::HOST_WRITE; break;
        case 2: buffer.final = CopyBufferState::COPY_SOURCE; break;
        case 3: ++buffer.size; break;
        case 4: buffer.transient->memory = MemoryType::CPU_ONLY; break;
        case 5: buffer.transient->usage = BufferUsageFlagBit::NONE; break;
        case 6: buffer.buffer = data.buffers[0].buffer; break;
        case 7: image.initialized = true; break;
        case 8: image.initial = ImageLayout::GENERAL; break;
        case 9: image.final = ImageLayout::TRANSFER_SRC; break;
        case 10: image.descriptor.memory = MemoryType::CPU_ONLY; break;
        case 11: image.descriptor.viewUsage = ImageViewUsageFlagBit::CUBE_MAP_COMPATIBLE; break;
        case 12: image.descriptor.usage |= ImageUsageFlagBit::DEPTH_STENCIL; break;
        }
        auto invalid = std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data));
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->Compile(lease, invalid))) << test;
    }
    EXPECT_EQ(Pool().GetStats().allocations, 0u);
    auto valid = Compile(*context, lease, graph);
    ASSERT_TRUE(valid);
    valid.reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(FrameTransient, SamePassBufferUsePreventsAliasing)
{
    auto  source = Host(true), output = Host(false);
    auto *bytes = source->Map();
    ASSERT_TRUE(bytes);
    std::memset(bytes, 93, 256);
    source->UnMap();
    FrameGraphBuilder builder;
    const auto        input  = Import(builder, source, true);
    const auto        result = Import(builder, output, false);
    const auto        first  = std::get<GraphBufferHandle>(builder.CreateTransient(BufferDesc()));
    const auto        second = std::get<GraphBufferHandle>(builder.CreateTransient(BufferDesc()));
    EXPECT_NE(first, second);
    builder.Copy({input, first, 0, 0, 256});
    builder.Copy({first, second, 0, 0, 256});
    builder.Copy({second, result, 0, 0, 256});
    builder.Export({result, 0, 256});
    auto graph = Freeze(builder);
    EXPECT_TRUE(std::holds_alternative<FrameError>(builder.CreateTransient(BufferDesc())));
    EXPECT_TRUE(std::holds_alternative<FrameError>(builder.CreateTransient(ImageDesc())));
    auto recording = Record(Compile(*context, Begin(*context), graph));
    ASSERT_TRUE(recording);
    EXPECT_EQ(Pool().GetStats().allocations, 2u);
    EXPECT_EQ(Pool().GetStats().reuses, 0u);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete();
    const auto *read = output->Map();
    ASSERT_TRUE(read);
    for (uint32_t i = 0; i < 256; ++i)
        EXPECT_EQ(read[i], 93);
    output->UnMap();
}

TEST_P(FrameTransient, BufferAliasUsesLiveIntervalsRatherThanDeclarationOrder)
{
    auto  source = Host(true), output = Host(false);
    auto *bytes = source->Map();
    ASSERT_TRUE(bytes);
    for (uint32_t i = 0; i < 3; ++i)
        std::memset(bytes + 64 * i, 31 + i, 64);
    source->UnMap();
    FrameGraphBuilder                builder;
    const auto                       input = Import(builder, source, true), result = Import(builder, output, false);
    std::array<GraphBufferHandle, 3> logical;
    for (auto &handle : logical)
        handle = std::get<GraphBufferHandle>(builder.CreateTransient(BufferDesc()));
    const auto     dead    = std::get<GraphBufferHandle>(builder.CreateTransient(BufferDesc()));
    const uint32_t order[] = {2, 0, 1};
    for (uint32_t i = 0; i < 3; ++i) {
        builder.Copy({input, logical[order[i]], 64 * i, 0, 64});
        builder.Copy({logical[order[i]], result, 0, 64 * i, 64});
    }
    builder.Copy({input, dead, 0, 0, 64});
    builder.Export({result, 0, 192});
    const auto graph    = Freeze(builder);
    const auto analyzed = AnalyzeFrameGraph(*graph);
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analyzed));
    const auto &analysis = std::get<FrameGraphAnalysis>(analyzed);
    EXPECT_EQ(analysis.bufferLifetimes[dead.index].first, UINT32_MAX);
    EXPECT_EQ(analysis.bufferLifetimes[logical[2].index].first, 0u);
    EXPECT_EQ(analysis.bufferLifetimes[logical[2].index].last, 1u);
    {
        FrameTransientPool limited(*device, {1, 256});
        auto               backing = limited.Acquire(graph->GetData(), analysis, info.deviceEpoch);
        ASSERT_TRUE(std::holds_alternative<FrameResourceBindings>(backing));
        EXPECT_EQ(limited.GetStats().objects, 1u);
        EXPECT_EQ(limited.GetStats().aliases, 2u);
        EXPECT_TRUE(std::holds_alternative<FrameError>(limited.Acquire(graph->GetData(), analysis, info.deviceEpoch)));
        EXPECT_EQ(limited.GetStats().borrowed, 1u);
    }
    for (uint32_t workers : {1u, 3u}) {
        context.reset();
        info.parallelNum = workers;
        context.reset(device->CreateFrameContext(info));
        auto plan = Compile(*context, Begin(*context), graph);
        ASSERT_TRUE(plan);
        const auto &bindings = Bindings(*plan);
        EXPECT_FALSE(bindings.buffers[dead.index]);
        for (auto handle : logical) {
            EXPECT_EQ(bindings.bufferBacking[handle.index], logical[2].index);
            EXPECT_EQ(bindings.buffers[handle.index].Get(), bindings.buffers[logical[2].index].Get());
        }
        auto recording = Record(std::move(plan));
        ASSERT_TRUE(recording);
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
        Complete();
        const auto *read = output->Map();
        ASSERT_TRUE(read);
        for (uint32_t i = 0; i < 192; ++i)
            EXPECT_EQ(read[i], 31 + i / 64);
        output->UnMap();
    }
    EXPECT_EQ(Pool().GetStats().allocations, 1u);
    EXPECT_EQ(Pool().GetStats().aliases, 4u);
    EXPECT_EQ(Pool().GetStats().borrowed, 0u);
}

TEST_P(FrameTransient, BufferAndImageAliasesKeepMipContentsAndOneFinalTransition)
{
    auto  source = Host(true), output = Host(false);
    auto *bytes = source->Map();
    ASSERT_TRUE(bytes);
    std::memset(bytes, 17, 64);
    std::memset(bytes + 64, 83, 64);
    source->UnMap();
    const auto graph = Graph(source, output, true);
    for (uint32_t frame = 0; frame < 3; ++frame) {
        auto plan = Compile(*context, Begin(*context), graph);
        ASSERT_TRUE(plan);
        const auto &bindings = Bindings(*plan);
        EXPECT_EQ(bindings.buffers[2].Get(), bindings.buffers[3].Get());
        EXPECT_EQ(bindings.images[0].Get(), bindings.images[1].Get());
        auto recording = Record(std::move(plan));
        ASSERT_TRUE(recording);
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
        Complete();
        const auto *read = output->Map();
        ASSERT_TRUE(read);
        for (uint32_t i = 0; i < 128; ++i)
            EXPECT_EQ(read[i], i < 64 ? 17 : 83);
        output->UnMap();
    }
    EXPECT_EQ(Pool().GetStats().allocations, 2u);
    EXPECT_EQ(Pool().GetStats().aliases, 6u);
    EXPECT_EQ(Pool().GetStats().reuses, 4u);
}

TEST_P(FrameTransient, SameStateAliasWritesHaveNativeMemoryDependency)
{
    auto  source = Host(true), output = Host(false);
    auto *bytes = source->Map();
    ASSERT_TRUE(bytes);
    std::memset(bytes, 29, 64);
    std::memset(bytes + 64, 101, 64);
    source->UnMap();
    FrameGraphBuilder builder;
    const auto        input = Import(builder, source, true), result = Import(builder, output, false);
    const auto        a = std::get<GraphBufferHandle>(builder.CreateTransient(BufferDesc()));
    const auto        b = std::get<GraphBufferHandle>(builder.CreateTransient(BufferDesc()));
    builder.Copy({input, a, 0, 0, 64, true});
    builder.Copy({input, b, 64, 0, 64});
    builder.Copy({b, result, 0, 0, 64});
    builder.Export({result, 0, 64});
    const auto                               graph = Freeze(builder);
    std::unique_ptr<BufferAliasBarrierProbe> probe;
    if (GetParam() != 2)
        probe = std::make_unique<BufferAliasBarrierProbe>(*static_cast<VulkanDevice *>(device));
    auto plan = Compile(*context, Begin(*context), graph);
    ASSERT_TRUE(plan);
    EXPECT_EQ(Bindings(*plan).buffers[a.index].Get(), Bindings(*plan).buffers[b.index].Get());
    if (GetParam() == 2) {
        const auto &before = static_cast<const D3D12CompiledFrame &>(*plan).encoder->tasks[1].before;
        ASSERT_EQ(before.size(), 2u);
        EXPECT_EQ(before[0].Transition.StateBefore, D3D12_RESOURCE_STATE_COPY_DEST);
        EXPECT_EQ(before[0].Transition.StateAfter, D3D12_RESOURCE_STATE_COMMON);
        EXPECT_EQ(before[1].Transition.StateBefore, D3D12_RESOURCE_STATE_COMMON);
        EXPECT_EQ(before[1].Transition.StateAfter, D3D12_RESOURCE_STATE_COPY_DEST);
    }
    auto recording = Record(std::move(plan));
    ASSERT_TRUE(recording);
    if (probe)
        EXPECT_GT(probe->writeDependencies, 0u);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete();
    const auto *read = output->Map();
    ASSERT_TRUE(read);
    for (uint32_t i = 0; i < 64; ++i)
        EXPECT_EQ(read[i], 101);
    output->UnMap();
}

TEST_P(FrameTransient, ColorAndDepthAliasPreserveScopesAndParallelDrawResults)
{
    auto output = Host(false, 512);
    auto red = ShaderOf(false), green = ShaderOf(true);
    ASSERT_TRUE(red && green);
    FrameGraphBuilder               builder;
    const auto                      result = Import(builder, output, false);
    std::array<GraphImageHandle, 2> colors, depths;
    for (uint32_t i = 0; i < 2; ++i) {
        auto desc = ImageDesc();
        desc.usage |= ImageUsageFlagBit::RENDER_TARGET;
        colors[i]   = std::get<GraphImageHandle>(builder.CreateTransient(desc));
        desc.format = PixelFormat::D32;
        desc.usage  = ImageUsageFlagBit::DEPTH_STENCIL;
        depths[i]   = std::get<GraphImageHandle>(builder.CreateTransient(desc));
        FrameRasterPass pass;
        pass.color = colors[i];
        pass.depth = RasterDepthAttachment{depths[i]};
        RasterDraw draw;
        draw.shader  = i ? green : red;
        draw.command = {3, 1, 0, 0};
        draw.depth   = {true, true, CompareOp::LESS_OR_EQUAL};
        pass.draws.assign(70, draw);
        builder.Raster(pass);
        pass.load = pass.depth->load = LoadOp::LOAD;
        pass.draws.resize(1);
        builder.Raster(pass);
        BufferImageCopy region;
        region.imageExtent  = {8, 8, 1};
        region.subRange     = {0, 0, 1, AspectFlagBit::COLOR_BIT};
        region.bufferOffset = 256 * i;
        builder.Copy({result, colors[i], region, ImageCopyDirection::IMAGE_TO_BUFFER});
    }
    builder.Export({result, 0, 512});
    const auto graph = Freeze(builder);
    for (uint32_t workers : {1u, 3u}) {
        context.reset();
        info.parallelNum = workers;
        context.reset(device->CreateFrameContext(info));
        auto plan = Compile(*context, Begin(*context), graph);
        ASSERT_TRUE(plan);
        EXPECT_EQ(Bindings(*plan).images[colors[0].index].Get(), Bindings(*plan).images[colors[1].index].Get());
        EXPECT_EQ(Bindings(*plan).images[depths[0].index].Get(), Bindings(*plan).images[depths[1].index].Get());
        if (GetParam() != 2) {
            const auto &passes = static_cast<const VulkanCompiledFrame &>(*plan).rasterPasses;
            ASSERT_EQ(passes.size(), 4u);
            EXPECT_NE(passes[0].nativeScope, passes[2].nativeScope);
            EXPECT_EQ(passes[0].nativeScope == passes[1].nativeScope, GetParam() == 1);
        }
        auto recording = Record(std::move(plan));
        ASSERT_TRUE(recording);
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
        Complete();
        const auto *read = output->Map();
        ASSERT_TRUE(read);
        for (uint32_t i = 0; i < 512; ++i)
            EXPECT_EQ(read[i], i % 4 == 3 || i % 4 == (i < 256 ? 0 : 1) ? 255 : 0);
        output->UnMap();
    }
    EXPECT_EQ(Pool().GetStats().allocations, 2u);
    EXPECT_EQ(Pool().GetStats().aliases, 4u);
}

TEST_P(FrameTransient, DiscardedAttachmentWritesStillParticipateInAliasSynchronization)
{
    auto              output = Host(false);
    FrameGraphBuilder builder;
    const auto        result = Import(builder, output, false);
    for (uint32_t i = 0; i < 2; ++i) {
        auto desc = ImageDesc();
        desc.usage |= ImageUsageFlagBit::RENDER_TARGET;
        const auto color      = std::get<GraphImageHandle>(builder.CreateTransient(desc));
        desc.format           = PixelFormat::D32;
        desc.usage            = ImageUsageFlagBit::DEPTH_STENCIL;
        const auto      depth = std::get<GraphImageHandle>(builder.CreateTransient(desc));
        FrameRasterPass pass;
        pass.color = color;
        pass.clear = ClearValue{0.f, 1.f, 0.f, 1.f};
        pass.depth = RasterDepthAttachment{depth};
        if (!i) {
            pass.store = pass.depth->store = StoreOp::DONT_CARE;
            pass.sideEffect                = true;
        }
        builder.Raster(pass);
        if (i) {
            BufferImageCopy region;
            region.imageExtent = {8, 8, 1};
            region.subRange    = {0, 0, 1, AspectFlagBit::COLOR_BIT};
            builder.Copy({result, color, region, ImageCopyDirection::IMAGE_TO_BUFFER});
        }
    }
    builder.Export({result, 0, 256});
    auto plan = Compile(*context, Begin(*context), Freeze(builder));
    ASSERT_TRUE(plan);
    EXPECT_EQ(Pool().GetStats().allocations, 2u);
    EXPECT_EQ(Pool().GetStats().aliases, 2u);
    auto recording = Record(std::move(plan));
    ASSERT_TRUE(recording);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete();
    const auto *read = output->Map();
    ASSERT_TRUE(read);
    for (uint32_t i = 0; i < 256; ++i)
        EXPECT_EQ(read[i], i % 4 == 1 || i % 4 == 3 ? 255 : 0);
    output->UnMap();
}

TEST_P(FrameTransient, ImageExportsAndOverlappingReadsPreventAliasing)
{
    for (bool exported : {false, true}) {
        FrameGraphBuilder builder;
        const auto        input = Import(builder, Host(true), true), result = Import(builder, Host(false, 512), false);
        const auto        a = std::get<GraphImageHandle>(builder.CreateTransient(ImageDesc()));
        const auto        b = std::get<GraphImageHandle>(builder.CreateTransient(ImageDesc()));
        BufferImageCopy   region;
        region.imageExtent = {8, 8, 1};
        region.subRange    = {0, 0, 1, AspectFlagBit::COLOR_BIT};
        builder.Copy({input, a, region});
        if (!exported)
            builder.Copy({input, b, region});
        builder.Copy({result, a, region, ImageCopyDirection::IMAGE_TO_BUFFER});
        if (exported) {
            builder.Export(a);
            builder.Copy({input, b, region});
        }
        region.bufferOffset = 256;
        builder.Copy({result, b, region, ImageCopyDirection::IMAGE_TO_BUFFER});
        builder.Export({result, 0, 512});
        const auto graph    = Freeze(builder);
        const auto analyzed = AnalyzeFrameGraph(*graph);
        ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analyzed));
        if (exported)
            EXPECT_EQ(std::get<FrameGraphAnalysis>(analyzed).imageLifetimes[a.index].last, graph->GetData().passes.size());
        const auto lease = Begin(*context);
        auto       plan  = Compile(*context, lease, graph);
        ASSERT_TRUE(plan);
        EXPECT_NE(Bindings(*plan).images[a.index].Get(), Bindings(*plan).images[b.index].Get());
        plan.reset();
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    }
    EXPECT_EQ(Pool().GetStats().aliases, 0u);
}

TEST_P(FrameTransient, NewLogicalOwnerCannotReadPreviousBufferBytesOrImageMip)
{
    const auto valid = Graph(Host(true), Host(false), true);
    const auto lease = Begin(*context);
    for (bool image : {false, true}) {
        auto data = valid->GetData();
        if (image) {
            std::get<BufferImageCopyPass>(data.passes[1]).region.subRange.baseLayer = 0;
            std::get<BufferImageCopyPass>(data.passes[2]).region.subRange.baseLayer = 0;
            auto &read                                                              = std::get<BufferImageCopyPass>(data.passes[5]);
            read.region.subRange.baseLayer                                          = 0;
        } else {
            auto &read               = std::get<BufferImageCopyPass>(data.passes[4]);
            read.region.bufferOffset = 64;
        }
        auto invalid = std::make_shared<const FrozenFrameGraph>(valid->GetId(), std::move(data));
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->Compile(lease, invalid)));
    }
    EXPECT_EQ(Pool().GetStats().allocations, 0u);
    auto plan = Compile(*context, lease, valid);
    ASSERT_TRUE(plan);
    EXPECT_EQ(Pool().GetStats().aliases, 2u);
}

TEST_P(FrameTransient, BufferExportsAndIncompatibleStoragePreventAliasing)
{
    for (uint32_t variant = 0; variant < 3; ++variant) {
        FrameGraphBuilder builder;
        const auto        input = Import(builder, Host(true), true), result = Import(builder, Host(false), false);
        const auto        a    = std::get<GraphBufferHandle>(builder.CreateTransient(BufferDesc()));
        auto              desc = BufferDesc();
        if (variant == 1)
            desc.size *= 2;
        if (variant == 2)
            desc.usage |= BufferUsageFlagBit::VERTEX;
        const auto b = std::get<GraphBufferHandle>(builder.CreateTransient(desc));
        builder.Copy({input, a, 0, 0, 64});
        builder.Copy({a, result, 0, 0, 64});
        builder.Copy({input, b, 0, 0, 64});
        builder.Copy({b, result, 0, 64, 64});
        builder.Export({result, 0, 128});
        if (!variant)
            builder.Export({a, 0, 64});
        const auto lease = Begin(*context);
        auto       plan  = Compile(*context, lease, Freeze(builder));
        ASSERT_TRUE(plan);
        EXPECT_NE(Bindings(*plan).buffers[a.index].Get(), Bindings(*plan).buffers[b.index].Get());
        plan.reset();
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    }
    EXPECT_EQ(Pool().GetStats().aliases, 0u);
}

INSTANTIATE_TEST_SUITE_P(NativeCompilers, FrameTransient, testing::Values(0u, 1u, 2u));
