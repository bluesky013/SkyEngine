//
// Created on 2026/10/16.
//

#include "GpuTestSupport.h"
#include "rdg/D3D12FrameCompiler.h"
#include "rdg/VulkanFrameCompiler.h"

#include <VulkanImage.h>
#include <VulkanInstance.h>
#include <mutex>

#include "graph/FrameGraphBuilder.h"
#include <D3D12Device.h>
#include <algorithm>
#include <aurora/rdg/DeviceFrameContext.h>
#include <d3d12sdklayers.h>
#include <utility>

namespace {
    struct ImageStateProbe {
        ImageStateProbe(VulkanDevice &device, VkImage image)
            : fn(const_cast<VulkanDeviceFunctions &>(device.GetDeviceFn())), saved(fn.vkCmdPipelineBarrier2), image(image)
        {
            active                   = this;
            fn.vkCmdPipelineBarrier2 = Barrier;
        }
        ~ImageStateProbe()
        {
            fn.vkCmdPipelineBarrier2 = saved;
            active                   = nullptr;
        }
        static void VKAPI_PTR Barrier(VkCommandBuffer command, const VkDependencyInfo *info)
        {
            {
                std::lock_guard lock(active->mutex);
                for (uint32_t i = 0; i < info->imageMemoryBarrierCount; ++i)
                    if (info->pImageMemoryBarriers[i].image == active->image)
                        active->barriers.push_back(info->pImageMemoryBarriers[i]);
            }
            active->saved(command, info);
        }
        inline static ImageStateProbe     *active = nullptr;
        VulkanDeviceFunctions             &fn;
        PFN_vkCmdPipelineBarrier2          saved;
        VkImage                            image;
        std::mutex                         mutex;
        std::vector<VkImageMemoryBarrier2> barriers;
    };
    class ImageSubresource : public testing::TestWithParam<uint32_t> {
    protected:
        void SetUp() override
        {
            Instance::Get()->Init({"ImageSubresourceTest", "SkyEngine", true, GetParam() == 2 ? API::DX12 : API::VULKAN});
            device = Instance::Get()->GetDevice();
            ASSERT_TRUE(device);
            if (GetParam() == 2) {
                ASSERT_TRUE(SUCCEEDED(static_cast<D3D12Device *>(device)->GetNativeHandle()->QueryInterface(IID_PPV_ARGS(&debug))));
                debug->ClearStoredMessages();
            }
            info.deviceEpoch    = 139;
            info.parallelNum    = 3;
            info.vulkanCompiler = GetParam() == 1 ? VulkanFrameGraphMode::RENDER_PASS : VulkanFrameGraphMode::DYNAMIC_RENDERING;
            ResetContext();
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
        void ResetContext()
        {
            context.reset();
            context.reset(device->CreateFrameContext(info));
            ASSERT_TRUE(context);
        }
        BufferPtr BufferOf(bool upload, uint64_t size = 1024)
        {
            Buffer::Descriptor desc;
            desc.size   = size;
            desc.memory = upload || GetParam() != 2 ? MemoryType::CPU_ONLY : MemoryType::GPU_TO_CPU;
            desc.usage  = upload ? BufferUsageFlagBit::TRANSFER_SRC : BufferUsageFlagBit::TRANSFER_DST;
            return BufferPtr(device->CreateBuffer(desc));
        }
        FrameGraphImage ImageOf()
        {
            FrameGraphImage image;
            image.descriptor.format      = PixelFormat::RGBA8_UNORM;
            image.descriptor.extent      = {9, 5, 1};
            image.descriptor.mipLevels   = 4;
            image.descriptor.arrayLayers = 2;
            image.descriptor.usage       = ImageUsageFlagBit::TRANSFER_SRC | ImageUsageFlagBit::TRANSFER_DST | ImageUsageFlagBit::RENDER_TARGET;
            image.image                  = ImagePtr(device->CreateImage(image.descriptor));
            EXPECT_TRUE(image.image);
            return image;
        }
        GraphBufferHandle Import(FrameGraphBuilder &builder, BufferPtr buffer, bool upload)
        {
            const auto state    = upload ? CopyBufferState::HOST_WRITE : CopyBufferState::HOST_READ;
            auto       imported = builder.Import(std::move(buffer), state, state, upload);
            EXPECT_TRUE(std::holds_alternative<GraphBufferHandle>(imported));
            auto *handle = std::get_if<GraphBufferHandle>(&imported);
            return handle ? *handle : GraphBufferHandle{};
        }
        GraphImageHandle Import(FrameGraphBuilder &builder, const FrameGraphImage &image)
        {
            auto imported = builder.Import(image);
            EXPECT_TRUE(std::holds_alternative<GraphImageHandle>(imported));
            auto *handle = std::get_if<GraphImageHandle>(&imported);
            return handle ? *handle : GraphImageHandle{};
        }
        BufferImageCopy Region(uint32_t mip, uint32_t layer, uint64_t offset = 0)
        {
            BufferImageCopy region;
            region.bufferOffset = offset;
            region.subRange     = {mip, layer, 1, AspectFlagBit::COLOR_BIT};
            region.imageExtent  = {std::max(1u, 9u >> mip), std::max(1u, 5u >> mip), 1};
            return region;
        }
        std::shared_ptr<const FrozenFrameGraph> Freeze(FrameGraphBuilder &builder)
        {
            auto frozen = builder.Freeze();
            EXPECT_TRUE(std::holds_alternative<std::shared_ptr<const FrozenFrameGraph>>(frozen));
            auto *graph = std::get_if<std::shared_ptr<const FrozenFrameGraph>>(&frozen);
            return graph ? std::move(*graph) : nullptr;
        }
        DeviceFrameLease Begin()
        {
            auto begin = context->TryBeginSlot();
            EXPECT_TRUE(std::holds_alternative<DeviceFrameLease>(begin));
            auto *lease = std::get_if<DeviceFrameLease>(&begin);
            return lease ? *lease : DeviceFrameLease{};
        }
        bool Execute(std::shared_ptr<const FrozenFrameGraph> graph, const std::function<void(const BackendCompiledFrame &)> &inspect = {})
        {
            auto  compiled = context->Compile(Begin(), graph);
            auto *plan     = std::get_if<std::unique_ptr<BackendCompiledFrame>>(&compiled);
            if (!plan) {
                ADD_FAILURE() << "Compile failed";
                return false;
            }
            if (inspect)
                inspect(**plan);
            graph.reset();
            auto  dispatched = context->Dispatch(*plan);
            auto *ticket     = std::get_if<EncodingTicket>(&dispatched);
            if (!ticket) {
                ADD_FAILURE() << "Dispatch failed";
                return false;
            }
            const auto deadline  = std::chrono::steady_clock::now() + 5s;
            auto       collected = context->TryCollect(*ticket);
            while (std::holds_alternative<FramePending>(collected) && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(1ms);
                collected = context->TryCollect(*ticket);
            }
            auto *recording = std::get_if<std::unique_ptr<RecordedFrame>>(&collected);
            if (!recording || !std::holds_alternative<SubmissionAccepted>(context->Submit(*recording))) {
                ADD_FAILURE() << "Recording/submission failed";
                return false;
            }
            std::array<FrameCompletion, 1> completed;
            while (std::chrono::steady_clock::now() < deadline) {
                if (context->PollCompleted(completed)) {
                    EXPECT_EQ(completed[0].status, FrameCompletionStatus::COMPLETE);
                    return completed[0].status == FrameCompletionStatus::COMPLETE;
                }
                std::this_thread::sleep_for(1ms);
            }
            ADD_FAILURE() << "Completion timed out";
            return false;
        }
        std::shared_ptr<const FrozenFrameGraph> PartialGraph()
        {
            FrameGraphBuilder builder;
            const auto        source = Import(builder, BufferOf(true), true);
            const auto        output = Import(builder, BufferOf(false), false);
            const auto        image  = Import(builder, ImageOf());
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, image, Region(1, 0)})));
            EXPECT_TRUE(
                std::holds_alternative<FrameOperationComplete>(builder.Copy({output, image, Region(1, 0), ImageCopyDirection::IMAGE_TO_BUFFER})));
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({output, 0, 32})));
            return Freeze(builder);
        }
        Device                             *device = nullptr;
        DeviceFrameContextInitInfo          info;
        std::unique_ptr<DeviceFrameContext> context;
        ComPtr<ID3D12InfoQueue>             debug;
    };
    class ImageState : public ImageSubresource {};
} // namespace

TEST(FrameGraphImports, ImagesUseOnePhysicalIdentityAndRejectConflictingDeclarations)
{
    FrameGraphBuilder builder;
    FrameGraphImage   image;
    image.image             = ImagePtr(new Image());
    image.descriptor.format = PixelFormat::RGBA8_UNORM;
    const auto imported     = builder.Import(image);
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(imported));
    const auto handle = std::get<GraphImageHandle>(imported);
    for (uint32_t i = 0; i < 4; ++i) {
        auto conflict = image;
        if (i == 0)
            conflict.initial = ImageLayout::TRANSFER_DST;
        else if (i == 1)
            conflict.final = ImageLayout::COLOR_ATTACHMENT;
        else if (i == 2)
            conflict.initialized = true;
        else
            ++conflict.descriptor.mipLevels;
        const auto rejected = builder.Import(conflict);
        ASSERT_TRUE(std::holds_alternative<FrameError>(rejected));
        EXPECT_EQ(std::get<FrameError>(rejected).code, FrameErrorCode::INVALID_ARGUMENT);
    }
    for (uint32_t i = 1; i < 256; ++i) {
        auto other  = image;
        other.image = ImagePtr(new Image());
        ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(builder.Import(other)));
    }
#if SKY_ENABLE_RESOURCE_NAME
    image.descriptor.name = "same storage";
#endif
    const auto duplicate = builder.Import(image);
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(duplicate));
    EXPECT_EQ(std::get<GraphImageHandle>(duplicate), handle);
    auto frozen = builder.Freeze();
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const FrozenFrameGraph>>(frozen));
    const auto &graph = std::get<std::shared_ptr<const FrozenFrameGraph>>(frozen);
    EXPECT_EQ(graph->GetData().images.size(), 256u);
    EXPECT_TRUE(std::holds_alternative<FrameError>(builder.Import(image)));
    auto data      = graph->GetData();
    data.images[1] = data.images[0];
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(graph->GetId(), std::move(data)))));
}

TEST_P(ImageSubresource, NonPowerOfTwoMipsAndLayersRoundTripInSerialAndParallel)
{
    for (const auto workers : {1u, 3u}) {
        info.parallelNum = workers;
        ResetContext();
        FrameGraphBuilder builder;
        auto              upload   = BufferOf(true);
        const auto        readback = BufferOf(false);
        ASSERT_TRUE(upload && readback);
        std::array<uint8_t, 1024> bytes;
        for (uint32_t i = 0; i < bytes.size(); ++i)
            bytes[i] = static_cast<uint8_t>((i * 17u + i / 13u) % 251u);
        auto *mapped = upload->Map();
        ASSERT_TRUE(mapped);
        std::memcpy(mapped, bytes.data(), bytes.size());
        upload->UnMap();
        const auto source      = Import(builder, upload, true);
        const auto destination = Import(builder, readback, false);
        auto       storage     = ImageOf();
        const auto image       = Import(builder, storage);
        EXPECT_EQ(Import(builder, storage), image);
        std::vector<std::pair<uint64_t, uint64_t>> ranges;
        uint64_t                                   offset = 4;
        for (uint32_t layer = 0; layer < 2; ++layer) {
            for (uint32_t mip = 0; mip < 4; ++mip) {
                const auto region = Region(mip, layer, offset);
                const auto size   = uint64_t{region.imageExtent.width} * region.imageExtent.height * 4;
                ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, image, region})));
                ASSERT_TRUE(
                    std::holds_alternative<FrameOperationComplete>(builder.Copy({destination, image, region, ImageCopyDirection::IMAGE_TO_BUFFER})));
                ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({destination, offset, size})));
                ranges.emplace_back(offset, size);
                offset += size + 4;
            }
        }
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export(image)));
        auto graph = Freeze(builder);
        storage.image = {};
        upload.Reset(nullptr);
        ASSERT_TRUE(Execute(std::move(graph)));
        mapped = readback->Map();
        ASSERT_TRUE(mapped);
        for (const auto &[begin, size] : ranges)
            EXPECT_EQ(std::memcmp(mapped + begin, bytes.data() + begin, size), 0) << begin << ", " << size;
        readback->UnMap();
    }
}

TEST_P(ImageSubresource, CullingTracksEachSubresourceAndEarlierReadVersions)
{
    FrameGraphBuilder builder;
    auto              upload   = BufferOf(true);
    const auto        readback = BufferOf(false);
    ASSERT_TRUE(upload && readback);
    auto *mapped = upload->Map();
    ASSERT_TRUE(mapped);
    std::fill_n(mapped, 32, uint8_t{31});
    std::fill_n(mapped + 32, 32, uint8_t{67});
    std::fill_n(mapped + 64, 32, uint8_t{113});
    upload->UnMap();
    const auto source     = Import(builder, upload, true);
    const auto output     = Import(builder, readback, false);
    auto       dead       = BufferOf(true);
    auto       deadImport = builder.Import(dead, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, false);
    ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(deadImport));
    const auto storage = ImageOf();
    const auto image   = Import(builder, storage);
    const auto again   = Import(builder, storage);
    ASSERT_EQ(image, again);
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({std::get<GraphBufferHandle>(deadImport), image, Region(1, 1)})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, image, Region(1, 1)})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, again, Region(2, 0, 32)})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, again, Region(1, 1, 4), ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, image, Region(1, 1, 64)})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, image, Region(1, 1, 40), ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, again, Region(2, 0, 80), ImageCopyDirection::IMAGE_TO_BUFFER})));
    for (const auto range : {GraphBufferExport{output, 4, 32}, GraphBufferExport{output, 40, 32}, GraphBufferExport{output, 80, 8}})
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export(range)));
    auto       graph    = Freeze(builder);
    const auto analyzed = AnalyzeFrameGraph(*graph);
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analyzed));
    EXPECT_EQ(std::get<FrameGraphAnalysis>(analyzed).livePasses, (std::vector<uint32_t>{1, 2, 3, 4, 5, 6}));
    ASSERT_TRUE(Execute(std::move(graph)));
    mapped = readback->Map();
    ASSERT_TRUE(mapped);
    for (uint32_t i = 0; i < 32; ++i) {
        EXPECT_EQ(mapped[4 + i], 31);
        EXPECT_EQ(mapped[40 + i], 113);
    }
    for (uint32_t i = 0; i < 8; ++i)
        EXPECT_EQ(mapped[80 + i], 67);
    readback->UnMap();
}

TEST_P(ImageSubresource, PartialProductionDoesNotDefineOtherMipsLayersOrWholeImage)
{
    const auto graph = PartialGraph();
    ASSERT_TRUE(graph);
    const auto lease = Begin();
    for (uint32_t fault = 0; fault < 3; ++fault) {
        auto  data = graph->GetData();
        auto &read = std::get<BufferImageCopyPass>(data.passes[1]);
        if (fault == 0)
            read.region = Region(2, 0);
        else if (fault == 1)
            read.region = Region(1, 1);
        else
            data.imageExports.push_back(read.image);
        auto result = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
        ASSERT_TRUE(std::holds_alternative<FrameError>(result));
        EXPECT_EQ(std::get<FrameError>(result).code, FrameErrorCode::COMPILE_FAILED);
        EXPECT_EQ(std::get<FrameError>(result).message, fault == 2 ? "Export reads undefined contents" : "Pass reads undefined contents");
    }
    auto data                  = graph->GetData();
    data.images[0].initial     = ImageLayout::TRANSFER_SRC;
    data.images[0].initialized = true;
    data.imageExports.push_back({graph->GetId(), 0});
    EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(FrozenFrameGraph(graph->GetId(), std::move(data)))));
    auto valid = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(valid));
    std::get<std::unique_ptr<BackendCompiledFrame>>(valid).reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(ImageSubresource, InvalidAndUnsupportedRegionsDoNotBorrowTheLease)
{
    const auto graph = PartialGraph();
    ASSERT_TRUE(graph);
    const auto lease = Begin();
    for (uint32_t fault = 0; fault < 16; ++fault) {
        SCOPED_TRACE(fault);
        auto  data     = graph->GetData();
        auto &region   = std::get<BufferImageCopyPass>(data.passes[0]).region;
        auto &desc     = data.images[0].descriptor;
        auto  expected = FrameErrorCode::COMPILE_FAILED;
        switch (fault) {
        case 0: region.subRange.level = UINT32_MAX; break;
        case 1: region.subRange.baseLayer = UINT32_MAX; break;
        case 2: region.subRange.layers = 0; break;
        case 3: desc.mipLevels = 0; break;
        case 4: desc.mipLevels = 33; break;
        case 5: desc.arrayLayers = 0; break;
        case 6: desc.arrayLayers = UINT32_MAX; break;
        case 7: region.imageExtent.width = 5; break;
        case 8: region.imageOffset.x = -1; break;
        case 9: region.bufferOffset = 2; break;
        case 10: region.bufferOffset = 1020; break;
        case 11: region.bufferRowLength = 3; break;
        case 12: region.imageExtent.width = 2; break;
        case 13:
            region.subRange.layers = 2;
            expected               = FrameErrorCode::UNSUPPORTED;
            break;
        case 14:
            desc.arrayLayers = 3;
            expected         = FrameErrorCode::INVALID_ARGUMENT;
            break;
        case 15: data.images[0].initialized = true; break;
        }
        auto result = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
        ASSERT_TRUE(std::holds_alternative<FrameError>(result));
        EXPECT_EQ(std::get<FrameError>(result).code, expected);
    }
    if (GetParam() == 2) {
        auto  data  = graph->GetData();
        auto &image = data.images[0];
        image.descriptor.arrayLayers += 65536u;
        image.image = ImagePtr(device->CreateImage(image.descriptor));
        ASSERT_TRUE(image.image);
        auto result = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
        ASSERT_TRUE(std::holds_alternative<FrameError>(result));
        EXPECT_EQ(std::get<FrameError>(result).code, FrameErrorCode::COMPILE_FAILED);
    }
    auto valid = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(valid));
    std::get<std::unique_ptr<BackendCompiledFrame>>(valid).reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(ImageSubresource, InvalidAttachmentViewsLeaveLeaseReusable)
{
    const auto graph = PartialGraph();
    ASSERT_TRUE(graph);
    const auto lease = Begin();
    for (const auto view : {RasterAttachmentView{4, 0}, RasterAttachmentView{0, 2}, RasterAttachmentView{UINT32_MAX, 0}}) {
        auto data = graph->GetData();
        data.passes.clear();
        data.exports.clear();
        FrameRasterPass pass;
        pass.color      = {graph->GetId(), 0};
        pass.colorView  = view;
        pass.sideEffect = true;
        data.passes.emplace_back(std::move(pass));
        auto result = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
        ASSERT_TRUE(std::holds_alternative<FrameError>(result));
        EXPECT_EQ(std::get<FrameError>(result).code, FrameErrorCode::COMPILE_FAILED);
    }
    auto valid = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(valid));
    std::get<std::unique_ptr<BackendCompiledFrame>>(valid).reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

INSTANTIATE_TEST_SUITE_P(Backends, ImageSubresource, testing::Values(0u, 1u, 2u));

TEST(FrameGraphImports, PartialContentsUseCanonicalUnionAndRejectInvalidRanges)
{
    FrameGraphBuilder builder;
    FrameGraphImage   image;
    image.image                  = ImagePtr(new Image());
    image.descriptor.extent      = {16, 8, 1};
    image.descriptor.mipLevels   = 4;
    image.descriptor.arrayLayers = 3;
    image.descriptor.format      = PixelFormat::RGBA8_UNORM;
    image.initial = image.final = ImageLayout::GENERAL;
    image.initializedRanges     = {{1, 2, 1, 2, AspectFlagBit::COLOR_BIT}};
    const auto resolved         = ResolveInitializedImageContents(image);
    ASSERT_TRUE(std::holds_alternative<ResolvedImageContents>(resolved));
    EXPECT_EQ(std::get<ResolvedImageContents>(resolved).ranges, (std::vector<ImageContentSpan>{{298, 338}, {468, 508}}));
    const auto first = builder.Import(image);
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(first));
    auto split              = image;
    split.initializedRanges = {{2, 1, 1, 2, AspectFlagBit::COLOR_BIT},
                               {1, 1, 2, 1, AspectFlagBit::COLOR_BIT},
                               {1, 1, 1, 1, AspectFlagBit::COLOR_BIT},
                               {1, 2, 1, 1, AspectFlagBit::COLOR_BIT}};
    const auto duplicate    = builder.Import(split);
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(duplicate));
    EXPECT_EQ(std::get<GraphImageHandle>(duplicate), std::get<GraphImageHandle>(first));
    split.initializedRanges.pop_back();
    split.initializedRanges.pop_back();
    EXPECT_TRUE(std::holds_alternative<FrameError>(builder.Import(split)));
    for (uint32_t fault = 0; fault < 10; ++fault) {
        auto  invalid = image;
        auto &range   = invalid.initializedRanges.front();
        switch (fault) {
        case 0: invalid.initialized = true; break;
        case 1: invalid.initial = ImageLayout::UNDEFINED; break;
        case 2: invalid.transient = true; break;
        case 3: range.levels = 0; break;
        case 4: range.layers = 0; break;
        case 5: range.baseLevel = UINT32_MAX; break;
        case 6: range.baseLayer = 3; break;
        case 7: range.layers = UINT32_MAX; break;
        case 8: range.aspectMask = AspectFlagBit::DEPTH_BIT; break;
        case 9: invalid.initializedRanges.resize(257, range); break;
        }
        EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveInitializedImageContents(invalid))) << fault;
        EXPECT_TRUE(std::holds_alternative<FrameError>(builder.Import(invalid))) << fault;
    }
    FrameGraphBuilder complete;
    image.initialized = true;
    image.initializedRanges.clear();
    const auto full = complete.Import(image);
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(full));
    image.initialized       = false;
    image.initializedRanges = {{0, 4, 0, 3, AspectFlagBit::COLOR_BIT}};
    const auto equivalent   = complete.Import(image);
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(equivalent));
    EXPECT_EQ(std::get<GraphImageHandle>(equivalent), std::get<GraphImageHandle>(full));
}

TEST(FrameGraphImports, ImageStateBudgetIsCheckedBeforeNativeAllocation)
{
    FrameGraphData data;
    for (uint32_t i = 0; i < 2; ++i) {
        FrameGraphImage image;
        image.image                  = ImagePtr(new Image());
        image.descriptor.format      = PixelFormat::RGBA8_UNORM;
        image.descriptor.arrayLayers = MAX_FRAME_IMAGE_SUBRESOURCES / 2 + 1;
        data.images.push_back(image);
    }
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    data.images[0].descriptor.arrayLayers = MAX_FRAME_IMAGE_SUBRESOURCES + 1;
    EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveInitializedImageContents(data.images[0])));
    for (auto &image : data.images)
        image.descriptor.arrayLayers = 1;
    EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(FrozenFrameGraph(1, std::move(data)))));
}

TEST_P(ImageState, PartialImportsKeepContentsAcrossGraphs)
{
    auto image   = ImageOf();
    image.final  = ImageLayout::GENERAL;
    auto  upload = BufferOf(true), readback = BufferOf(false);
    auto *bytes = upload->Map();
    ASSERT_TRUE(bytes);
    std::fill_n(bytes, 32, uint8_t{31});
    std::fill_n(bytes + 32, 8, uint8_t{77});
    std::fill_n(bytes + 64, 8, uint8_t{113});
    upload->UnMap();
    FrameGraphBuilder initial;
    const auto        source = Import(initial, upload, true);
    const auto        target = Import(initial, image);
    ASSERT_TRUE(
        std::holds_alternative<FrameOperationComplete>(initial.Copy({source, target, Region(1, 0), ImageCopyDirection::BUFFER_TO_IMAGE, true})));
    ASSERT_TRUE(
        std::holds_alternative<FrameOperationComplete>(initial.Copy({source, target, Region(2, 1, 32), ImageCopyDirection::BUFFER_TO_IMAGE, true})));
    ASSERT_TRUE(Execute(Freeze(initial)));
    image.initial           = ImageLayout::GENERAL;
    image.initializedRanges = {{1, 1, 0, 1, AspectFlagBit::COLOR_BIT}, {2, 1, 1, 1, AspectFlagBit::COLOR_BIT}};
    FrameGraphBuilder next;
    const auto        sampled = Import(next, image);
    const auto        output  = Import(next, readback, false);
    const auto        updates = Import(next, upload, true);
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(next.Copy({output, sampled, Region(1, 0), ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(next.Copy({updates, sampled, Region(2, 0, 64)})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(next.Copy({output, sampled, Region(2, 1, 32), ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(next.Copy({output, sampled, Region(2, 0, 64), ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(next.Export({output, 0, 40})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(next.Export({output, 64, 8})));
    const auto graph = Freeze(next);
    const auto lease = Begin();
    for (uint32_t fault = 0; fault < 3; ++fault) {
        auto data = graph->GetData();
        if (fault == 0)
            data.images[0].initializedRanges.pop_back();
        else if (fault == 1)
            data.imageExports.push_back(sampled);
        else
            data.images[0].initial = ImageLayout::UNDEFINED;
        EXPECT_TRUE(
            std::holds_alternative<FrameError>(context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)))));
    }
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    ASSERT_TRUE(Execute(graph));
    bytes = readback->Map();
    ASSERT_TRUE(bytes);
    for (uint32_t i = 0; i < 32; ++i)
        EXPECT_EQ(bytes[i], 31);
    for (uint32_t i = 32; i < 40; ++i)
        EXPECT_EQ(bytes[i], 77);
    for (uint32_t i = 64; i < 72; ++i)
        EXPECT_EQ(bytes[i], 113);
    readback->UnMap();
}

TEST_P(ImageState, NativeBarriersTrackTouchedSubresourcesAndFinalBoundary)
{
    auto image   = ImageOf();
    image.final  = ImageLayout::GENERAL;
    auto  upload = BufferOf(true), readback = BufferOf(false);
    auto *bytes = upload->Map();
    ASSERT_TRUE(bytes);
    std::fill_n(bytes, 32, uint8_t{31});
    std::fill_n(bytes + 32, 8, uint8_t{77});
    upload->UnMap();
    FrameGraphBuilder builder;
    const auto        source = Import(builder, upload, true), output = Import(builder, readback, false);
    const auto        handle = Import(builder, image);
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, handle, Region(1, 0)})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, handle, Region(1, 0), ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, handle, Region(2, 1, 32)})));
    ASSERT_TRUE(
        std::holds_alternative<FrameOperationComplete>(builder.Copy({output, handle, Region(1, 0, 64), ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(
        std::holds_alternative<FrameOperationComplete>(builder.Copy({output, handle, Region(2, 1, 96), ImageCopyDirection::IMAGE_TO_BUFFER})));
    for (const auto range : {std::pair{0u, 32u}, std::pair{64u, 32u}, std::pair{96u, 8u}})
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({output, range.first, range.second})));
    std::unique_ptr<ImageStateProbe> probe;
    if (GetParam() != 2)
        probe =
            std::make_unique<ImageStateProbe>(*static_cast<VulkanDevice *>(device), static_cast<VulkanImage *>(image.image.Get())->GetNativeHandle());
    ASSERT_TRUE(Execute(Freeze(builder), [&](const BackendCompiledFrame &plan) {
        if (GetParam() != 2)
            return;
        const auto &tasks  = static_cast<const D3D12CompiledFrame &>(plan).encoder->tasks;
        auto       *native = static_cast<D3D12Image *>(image.image.Get())->GetNativeHandle();
        ASSERT_EQ(tasks.size(), 5u);
        std::vector<uint32_t> changed;
        for (uint32_t i = 0; i < tasks.size(); ++i)
            for (const auto &barrier : tasks[i].before)
                if (barrier.Transition.pResource == native) {
                    EXPECT_NE(barrier.Transition.Subresource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
                    EXPECT_NE(i, 3u);
                    changed.push_back(barrier.Transition.Subresource);
                }
        EXPECT_EQ(changed, (std::vector<uint32_t>{1, 1, 6, 6}));
        std::vector<uint32_t> finished;
        for (const auto &barrier : tasks.back().after)
            if (barrier.Transition.pResource == native) {
                EXPECT_EQ(barrier.Transition.StateAfter, D3D12_RESOURCE_STATE_COMMON);
                finished.push_back(barrier.Transition.Subresource);
            }
        EXPECT_EQ(finished, (std::vector<uint32_t>{1, 6}));
    }));
    if (probe) {
        std::array<VkImageLayout, 8> states{};
        bool                         finishing = false;
        for (const auto &barrier : probe->barriers) {
            const auto &range = barrier.subresourceRange;
            finishing         = finishing || barrier.newLayout == VK_IMAGE_LAYOUT_GENERAL;
            if (!finishing) {
                EXPECT_EQ(range.levelCount, 1u);
                EXPECT_EQ(range.layerCount, 1u);
                const auto sub = range.baseArrayLayer * 4 + range.baseMipLevel;
                EXPECT_TRUE(sub == 1 || sub == 6);
            }
            for (uint32_t layer = range.baseArrayLayer; layer < range.baseArrayLayer + range.layerCount; ++layer)
                for (uint32_t mip = range.baseMipLevel; mip < range.baseMipLevel + range.levelCount; ++mip) {
                    const auto sub = layer * 4 + mip;
                    ASSERT_LT(sub, states.size());
                    EXPECT_EQ(barrier.oldLayout, states[sub]);
                    states[sub] = barrier.newLayout;
                }
        }
        EXPECT_TRUE(finishing);
        for (const auto state : states)
            EXPECT_EQ(state, VK_IMAGE_LAYOUT_GENERAL);
    }
    bytes = readback->Map();
    ASSERT_TRUE(bytes);
    EXPECT_EQ(bytes[0], 31);
    EXPECT_EQ(bytes[64], 31);
    EXPECT_EQ(bytes[96], 77);
    readback->UnMap();
}

TEST_P(ImageState, AliasedImagesRetainPhysicalSubresourceStatesWithoutSharingContents)
{
    auto  upload = BufferOf(true), readback = BufferOf(false);
    auto *bytes = upload->Map();
    ASSERT_TRUE(bytes);
    std::fill_n(bytes, 32, uint8_t{31});
    std::fill_n(bytes + 32, 32, uint8_t{77});
    std::fill_n(bytes + 64, 8, uint8_t{113});
    upload->UnMap();
    FrameGraphBuilder builder;
    const auto        source = Import(builder, upload, true), output = Import(builder, readback, false);
    const auto        desc  = ImageOf().descriptor;
    const auto        first = builder.CreateTransient(desc), second = builder.CreateTransient(desc);
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(first) && std::holds_alternative<GraphImageHandle>(second));
    const auto a = std::get<GraphImageHandle>(first), b = std::get<GraphImageHandle>(second);
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, a, Region(1, 0)})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, a, Region(1, 0), ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, b, Region(1, 0, 32)})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, b, Region(1, 0, 32), ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, b, Region(2, 1, 64)})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, b, Region(2, 1, 64), ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({output, 0, 72})));
    const auto graph   = Freeze(builder);
    auto       invalid = graph->GetData();
    invalid.passes.erase(invalid.passes.begin() + 2);
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(graph->GetId(), std::move(invalid)))));
    ASSERT_TRUE(Execute(graph, [&](const BackendCompiledFrame &plan) {
        const auto &resources =
            GetParam() == 2 ? static_cast<const D3D12CompiledFrame &>(plan).resources : static_cast<const VulkanCompiledFrame &>(plan).resources;
        EXPECT_EQ(resources.imageBacking[a.index], resources.imageBacking[b.index]);
        EXPECT_EQ(resources.images[a.index].Get(), resources.images[b.index].Get());
    }));
    bytes = readback->Map();
    ASSERT_TRUE(bytes);
    EXPECT_EQ(bytes[0], 31);
    EXPECT_EQ(bytes[32], 77);
    EXPECT_EQ(bytes[64], 113);
    readback->UnMap();
}

INSTANTIATE_TEST_SUITE_P(Backends, ImageState, testing::Values(0u, 1u, 2u));

namespace {
    class ImageRectangle : public ImageSubresource {};
} // namespace

TEST_P(ImageRectangle, PaddedRectangleRoundTripPreservesBufferCanaries)
{
    for (uint32_t workers : {1u, 3u}) {
        info.parallelNum = workers;
        ResetContext();
        auto                      sourceBuffer = BufferOf(true), outputBuffer = BufferOf(false);
        std::array<uint8_t, 1024> expected;
        for (uint32_t i = 0; i < expected.size(); ++i)
            expected[i] = static_cast<uint8_t>((i * 17 + i / 13) % 251);
        std::fill(expected.begin() + 512, expected.begin() + 768, uint8_t{219});
        auto *mapped = sourceBuffer->Map();
        ASSERT_TRUE(mapped);
        std::memcpy(mapped, expected.data(), expected.size());
        sourceBuffer->UnMap();
        FrameGraphBuilder builder;
        const auto        source = Import(builder, sourceBuffer, true), output = Import(builder, outputBuffer, false);
        const auto        image = Import(builder, ImageOf());
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, output, 512, 0, 256})));
        auto upload                = Region(0, 1, 12);
        upload.imageOffset         = {2, 1, 0};
        upload.imageExtent         = {3, 3, 1};
        upload.bufferRowLength     = 7;
        upload.bufferImageHeight   = 9;
        auto download              = upload;
        download.bufferOffset      = 20;
        download.bufferRowLength   = 8;
        download.bufferImageHeight = 7;
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, image, upload})));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, image, download, ImageCopyDirection::IMAGE_TO_BUFFER})));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({output, 0, 256})));
        auto graph = Freeze(builder);
        ASSERT_TRUE(graph);
        ASSERT_TRUE(Execute(graph, [&](const BackendCompiledFrame &plan) {
            if (GetParam() != 2)
                return;
            const auto &tasks = static_cast<const D3D12CompiledFrame &>(plan).encoder->tasks;
            for (const auto &task : tasks)
                if (const auto *copy = std::get_if<D3D12ImageCopyStep>(&task.operation)) {
                    EXPECT_EQ(copy->footprint.Footprint.Width, 3u);
                    EXPECT_EQ(copy->footprint.Footprint.Height, 3u);
                    EXPECT_EQ(copy->footprint.Footprint.RowPitch % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, 0u);
                    EXPECT_EQ(copy->footprint.Offset % D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, 0u);
                }
        }));
        mapped = outputBuffer->Map();
        ASSERT_TRUE(mapped);
        for (uint32_t i = 0; i < 256; ++i) {
            uint8_t value = 219;
            for (uint32_t row = 0; row < 3; ++row)
                if (i >= 20 + row * 32 && i < 32 + row * 32)
                    value = expected[12 + row * 28 + i - (20 + row * 32)];
            EXPECT_EQ(mapped[i], value) << i;
        }
        outputBuffer->UnMap();
    }
}

TEST_P(ImageRectangle, TilesAndOverlappingPatchKeepEarlierVersionsAndCullDeadProducer)
{
    auto  sourceBuffer = BufferOf(true), outputBuffer = BufferOf(false);
    auto *mapped = sourceBuffer->Map();
    ASSERT_TRUE(mapped);
    std::fill_n(mapped, 24, uint8_t{31});
    std::fill_n(mapped + 24, 24, uint8_t{77});
    std::fill_n(mapped + 128, 8, uint8_t{113});
    sourceBuffer->UnMap();
    FrameGraphBuilder builder;
    const auto        source = Import(builder, sourceBuffer, true), output = Import(builder, outputBuffer, false);
    auto              desc = ImageOf().descriptor;
    desc.extent            = {4, 3, 1};
    desc.mipLevels = desc.arrayLayers = 1;
    const auto created                = builder.CreateTransient(desc);
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(created));
    const auto image         = std::get<GraphImageHandle>(created);
    const auto uninitialized = builder.Import(BufferOf(true), CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, false);
    ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(uninitialized));
    auto left                = Region(0, 0);
    left.imageExtent         = {2, 3, 1};
    auto right               = left;
    right.imageOffset.x      = 2;
    right.bufferOffset       = 24;
    auto full                = left;
    full.imageExtent.width   = 4;
    auto patch               = left;
    patch.imageOffset        = {1, 1, 0};
    patch.imageExtent.height = 1;
    patch.bufferOffset       = 128;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({std::get<GraphBufferHandle>(uninitialized), image, left})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, image, left})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, image, right})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, image, full, ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, image, patch})));
    full.bufferOffset = 64;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, image, full, ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({output, 0, 48})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({output, 64, 48})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export(image)));
    const auto graph = Freeze(builder);
    ASSERT_TRUE(graph);
    const auto result = AnalyzeFrameGraph(*graph);
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(result));
    EXPECT_EQ(std::get<FrameGraphAnalysis>(result).livePasses, (std::vector<uint32_t>{1, 2, 3, 4, 5}));
    auto gap                                                              = graph->GetData();
    std::get<BufferImageCopyPass>(gap.passes[2]).region.imageExtent.width = 1;
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(graph->GetId(), std::move(gap)))));
    ASSERT_TRUE(Execute(graph));
    mapped = outputBuffer->Map();
    ASSERT_TRUE(mapped);
    for (uint32_t row = 0; row < 3; ++row)
        for (uint32_t col = 0; col < 4; ++col)
            for (uint32_t channel = 0; channel < 4; ++channel) {
                const auto offset  = (row * 4 + col) * 4 + channel;
                const auto initial = col < 2 ? 31 : 77;
                EXPECT_EQ(mapped[offset], initial);
                EXPECT_EQ(mapped[64 + offset], row == 1 && col >= 1 && col < 3 ? 113 : initial);
            }
    outputBuffer->UnMap();
}

TEST_P(ImageRectangle, BufferPaddingIsNeitherReadNorDefined)
{
    auto  sourceBuffer = BufferOf(true), outputBuffer = BufferOf(false);
    auto *mapped = sourceBuffer->Map();
    ASSERT_TRUE(mapped);
    for (uint32_t i = 0; i < 12; ++i)
        mapped[i] = static_cast<uint8_t>(31 + i);
    sourceBuffer->UnMap();
    FrameGraphBuilder  builder;
    const auto         source = Import(builder, sourceBuffer, true), output = Import(builder, outputBuffer, false);
    Buffer::Descriptor desc;
    desc.size          = 32;
    desc.usage         = BufferUsageFlagBit::TRANSFER_SRC | BufferUsageFlagBit::TRANSFER_DST;
    const auto created = builder.CreateTransient(desc);
    ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(created));
    const auto padded = std::get<GraphBufferHandle>(created);
    for (uint32_t row = 0; row < 3; ++row)
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, padded, row * 4u, 4 + row * 12u, 4})));
    const auto image       = Import(builder, ImageOf());
    auto       region      = Region(0, 1, 4);
    region.imageOffset     = {8, 2, 0};
    region.imageExtent     = {1, 3, 1};
    region.bufferRowLength = 3;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({padded, image, region})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, image, region, ImageCopyDirection::IMAGE_TO_BUFFER})));
    for (uint32_t row = 0; row < 3; ++row)
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({output, 4 + row * 12u, 4})));
    const auto graph = Freeze(builder);
    ASSERT_TRUE(graph);
    for (uint32_t fault = 0; fault < 3; ++fault) {
        auto data = graph->GetData();
        if (fault == 0)
            data.exports.push_back({output, 8, 8});
        else if (fault == 1)
            data.exports.push_back({output, 32, 4});
        else
            data.passes.erase(data.passes.begin() + 1);
        EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(graph->GetId(), std::move(data)))));
    }
    ASSERT_TRUE(Execute(graph));
    mapped = outputBuffer->Map();
    ASSERT_TRUE(mapped);
    for (uint32_t row = 0; row < 3; ++row)
        for (uint32_t byte = 0; byte < 4; ++byte)
            EXPECT_EQ(mapped[4 + row * 12 + byte], 31 + row * 4 + byte);
    outputBuffer->UnMap();
}

TEST_P(ImageRectangle, InvalidPitchesBoundsAndOverflowLeaveLeaseReusable)
{
    const auto graph = PartialGraph();
    ASSERT_TRUE(graph);
    const auto lease = Begin();
    for (uint32_t fault = 0; fault < 8; ++fault) {
        auto  data   = graph->GetData();
        auto &region = std::get<BufferImageCopyPass>(data.passes[0]).region;
        switch (fault) {
        case 0: region.bufferRowLength = 3; break;
        case 1: region.bufferImageHeight = 1; break;
        case 2: region.bufferRowLength = UINT32_MAX; break;
        case 3: region.bufferOffset = UINT64_MAX - 3; break;
        case 4: region.bufferRowLength = 256; break;
        case 5: region.imageOffset.x = 1; break;
        case 6: region.imageOffset.y = 1; break;
        case 7: region.imageOffset.y = INT32_MAX; break;
        }
        const auto result = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
        ASSERT_TRUE(std::holds_alternative<FrameError>(result)) << fault;
        EXPECT_EQ(std::get<FrameError>(result).code, FrameErrorCode::COMPILE_FAILED) << fault;
    }
    auto valid = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(valid));
    std::get<std::unique_ptr<BackendCompiledFrame>>(valid).reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST(FrameGraphImports, RectangleRangesAndImageAddressesAreBoundedBeforeNativeAllocation)
{
    FrameGraphData  data;
    FrameGraphImage image;
    image.image             = ImagePtr(new Image());
    image.descriptor.format = PixelFormat::RGBA8_UNORM;
    image.descriptor.extent = {2, MAX_FRAME_IMAGE_COPY_RANGES, 1};
    data.images.push_back(image);
    FrameGraphBuffer buffer;
    buffer.buffer      = BufferPtr(new Buffer());
    buffer.size        = uint64_t(MAX_FRAME_IMAGE_COPY_RANGES) * 4;
    buffer.initialized = true;
    data.buffers.push_back(buffer);
    BufferImageCopyPass pass;
    pass.buffer             = {1, 0};
    pass.image              = {1, 0};
    pass.region.imageExtent = {1, MAX_FRAME_IMAGE_COPY_RANGES, 1};
    pass.sideEffect         = true;
    data.passes.push_back(pass);
    auto result = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameError>(result));
    EXPECT_EQ(std::get<FrameError>(result).message, "Image copy content range budget exceeded");
    data.passes.clear();
    data.images[0].descriptor.extent      = {UINT32_MAX, 1073741824, 1};
    data.images[0].descriptor.arrayLayers = 5;
    result                                = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameError>(result));
    EXPECT_EQ(std::get<FrameError>(result).message, "Image content address overflow");
    data.images[0].descriptor.arrayLayers = 1;
    EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(FrozenFrameGraph(1, std::move(data)))));
}

TEST_P(ImageRectangle, PartialPatchPreservesInitializedMipAcrossGraphs)
{
    auto image         = ImageOf();
    image.final        = ImageLayout::GENERAL;
    auto  sourceBuffer = BufferOf(true), outputBuffer = BufferOf(false);
    auto *mapped = sourceBuffer->Map();
    ASSERT_TRUE(mapped);
    std::fill_n(mapped, 180, uint8_t{31});
    std::fill_n(mapped + 192, 36, uint8_t{77});
    sourceBuffer->UnMap();
    FrameGraphBuilder first;
    const auto        source = Import(first, sourceBuffer, true);
    const auto        target = Import(first, image);
    ASSERT_TRUE(
        std::holds_alternative<FrameOperationComplete>(first.Copy({source, target, Region(0, 1), ImageCopyDirection::BUFFER_TO_IMAGE, true})));
    ASSERT_TRUE(Execute(Freeze(first)));
    image.initial           = ImageLayout::GENERAL;
    image.initializedRanges = {{0, 1, 1, 1, AspectFlagBit::COLOR_BIT}};
    FrameGraphBuilder second;
    const auto        nextSource = Import(second, sourceBuffer, true), output = Import(second, outputBuffer, false);
    const auto        nextTarget = Import(second, image);
    auto              patch      = Region(0, 1, 192);
    patch.imageOffset            = {2, 1, 0};
    patch.imageExtent            = {3, 3, 1};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(second.Copy({nextSource, nextTarget, patch})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(second.Copy({output, nextTarget, Region(0, 1), ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(second.Export({output, 0, 180})));
    ASSERT_TRUE(Execute(Freeze(second), [&](const BackendCompiledFrame &plan) {
        if (GetParam() == 2)
            for (const auto &task : static_cast<const D3D12CompiledFrame &>(plan).encoder->tasks)
                if (std::holds_alternative<D3D12ImageCopyStep>(task.operation))
                    EXPECT_FALSE(task.initializeImage);
    }));
    mapped = outputBuffer->Map();
    ASSERT_TRUE(mapped);
    for (uint32_t row = 0; row < 5; ++row)
        for (uint32_t col = 0; col < 9; ++col)
            for (uint32_t channel = 0; channel < 4; ++channel)
                EXPECT_EQ(mapped[(row * 9 + col) * 4 + channel], row >= 1 && row < 4 && col >= 2 && col < 5 ? 77 : 31);
    outputBuffer->UnMap();
}

TEST_P(ImageRectangle, AliasedRectangleWritesDoNotInheritLogicalContents)
{
    auto  sourceBuffer = BufferOf(true), outputBuffer = BufferOf(false);
    auto *mapped = sourceBuffer->Map();
    ASSERT_TRUE(mapped);
    std::fill_n(mapped, 16, uint8_t{31});
    std::fill_n(mapped + 16, 16, uint8_t{77});
    sourceBuffer->UnMap();
    FrameGraphBuilder builder;
    const auto        source = Import(builder, sourceBuffer, true), output = Import(builder, outputBuffer, false);
    const auto        desc   = ImageOf().descriptor;
    const auto        a      = std::get<GraphImageHandle>(builder.CreateTransient(desc));
    const auto        b      = std::get<GraphImageHandle>(builder.CreateTransient(desc));
    auto              region = Region(1, 1);
    region.imageExtent.width = 2;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, a, region})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, a, region, ImageCopyDirection::IMAGE_TO_BUFFER})));
    region.bufferOffset  = 16;
    region.imageOffset.x = 2;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, b, region})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, b, region, ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({output, 0, 32})));
    const auto graph = Freeze(builder);
    ASSERT_TRUE(graph);
    auto invalid                                            = graph->GetData();
    std::get<BufferImageCopyPass>(invalid.passes[3]).region = Region(1, 1, 16);
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(graph->GetId(), std::move(invalid)))));
    ASSERT_TRUE(Execute(graph, [&](const BackendCompiledFrame &plan) {
        const auto &resources =
            GetParam() == 2 ? static_cast<const D3D12CompiledFrame &>(plan).resources : static_cast<const VulkanCompiledFrame &>(plan).resources;
        EXPECT_EQ(resources.images[a.index].Get(), resources.images[b.index].Get());
        if (GetParam() == 2) {
            uint32_t discards = 0;
            for (const auto &task : static_cast<const D3D12CompiledFrame &>(plan).encoder->tasks)
                if (std::holds_alternative<D3D12ImageCopyStep>(task.operation))
                    discards += task.initializeImage.has_value();
            EXPECT_EQ(discards, 1u);
        }
    }));
    mapped = outputBuffer->Map();
    ASSERT_TRUE(mapped);
    for (uint32_t i = 0; i < 32; ++i)
        EXPECT_EQ(mapped[i], i < 16 ? 31 : 77);
    outputBuffer->UnMap();
}

INSTANTIATE_TEST_SUITE_P(Backends, ImageRectangle, testing::Values(0u, 1u, 2u));

TEST(FrameGraphImports, RectangleImportsCompareCoverageRatherThanDeclarationShape)
{
    FrameGraphImage image;
    image.image                = ImagePtr(new Image());
    image.descriptor.format    = PixelFormat::RGBA8_UNORM;
    image.descriptor.extent    = {8, 4, 1};
    image.descriptor.mipLevels = image.descriptor.arrayLayers = 2;
    image.initial = image.final = ImageLayout::GENERAL;
    image.initializedRanges     = {{1, 1, 0, 1, AspectFlagBit::COLOR_BIT}};
    image.initializedRegions    = {{0, 1, {{0, 0}, {4, 4}}}};
    const auto partial          = ResolveInitializedImageContents(image);
    ASSERT_TRUE(std::holds_alternative<ResolvedImageContents>(partial));
    EXPECT_EQ(std::get<ResolvedImageContents>(partial).subresourceOffsets, (std::vector<uint64_t>{0, 32, 40, 72, 80}));
    EXPECT_EQ(std::get<ResolvedImageContents>(partial).ranges, (std::vector<ImageContentSpan>{{32, 44}, {48, 52}, {56, 60}, {64, 68}}));
    image.initializedRegions.push_back({0, 1, {{4, 0}, {4, 4}}});
    FrameGraphBuilder builder;
    const auto        first = builder.Import(image);
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(first));
    auto equivalent = image;
    equivalent.initializedRegions.clear();
    equivalent.initializedRanges.push_back({0, 1, 1, 1, AspectFlagBit::COLOR_BIT});
    const auto second = builder.Import(equivalent);
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(second));
    EXPECT_EQ(std::get<GraphImageHandle>(first), std::get<GraphImageHandle>(second));
    std::reverse(image.initializedRegions.begin(), image.initializedRegions.end());
    image.initializedRegions.push_back({0, 1, {{2, 1}, {4, 2}}});
    EXPECT_TRUE(std::holds_alternative<GraphImageHandle>(builder.Import(image)));
    image.initializedRegions.pop_back();
    --image.initializedRegions.front().rectangle.extent.height;
    EXPECT_TRUE(std::holds_alternative<FrameError>(builder.Import(image)));
}

TEST(FrameGraphImports, RectangleImportValidationCannotBeBypassedByFrozenDeclarations)
{
    FrameGraphImage image;
    image.image                = ImagePtr(new Image());
    image.descriptor.format    = PixelFormat::RGBA8_UNORM;
    image.descriptor.extent    = {8, 4, 1};
    image.descriptor.mipLevels = image.descriptor.arrayLayers = 2;
    image.initial = image.final = ImageLayout::GENERAL;
    image.initializedRegions    = {{1, 1, {{1, 0}, {2, 2}}}};
    for (uint32_t fault = 0; fault < 21; ++fault) {
        auto  invalid = image;
        auto &region  = invalid.initializedRegions.front();
        switch (fault) {
        case 0: region.rectangle.offset.x = -1; break;
        case 1: region.rectangle.offset.y = -1; break;
        case 2: region.rectangle.extent.width = 0; break;
        case 3: region.rectangle.extent.height = 0; break;
        case 4: region.rectangle.offset.x = 4; break;
        case 5: region.rectangle.offset.y = 2; break;
        case 6: region.rectangle.extent.width = UINT32_MAX; break;
        case 7: region.rectangle.extent.height = UINT32_MAX; break;
        case 8: region.mip = UINT32_MAX; break;
        case 9: region.layer = 2; break;
        case 10: invalid.initialized = true; break;
        case 11: invalid.initial = ImageLayout::UNDEFINED; break;
        case 12: invalid.transient = true; break;
        case 13: invalid.descriptor.format = PixelFormat::D32; break;
        case 14: invalid.descriptor.samples = SampleCount::X2; break;
        case 15: invalid.descriptor.extent.depth = 2; break;
        case 16: invalid.descriptor.imageType = ImageType::IMAGE_3D; break;
        case 17: invalid.initializedRegions.resize(257, region); break;
        case 18: invalid.descriptor.mipLevels = 33; break;
        case 19: invalid.descriptor.extent.width = 0; break;
        case 20: invalid.initializedRanges.resize(256, {0, 1, 0, 1, AspectFlagBit::COLOR_BIT}); break;
        }
        EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveInitializedImageContents(invalid))) << fault;
        FrameGraphBuilder builder;
        EXPECT_TRUE(std::holds_alternative<FrameError>(builder.Import(invalid))) << fault;
        FrameGraphData data;
        data.images.push_back(std::move(invalid));
        EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, std::move(data))))) << fault;
    }
}

TEST(FrameGraphImports, InitialRectangleExpansionSharesOneGraphBudgetEvenForUnusedImages)
{
    FrameGraphImage image;
    image.image             = ImagePtr(new Image());
    image.descriptor.format = PixelFormat::RGBA8_UNORM;
    image.descriptor.extent = {2, MAX_FRAME_IMAGE_INITIAL_RANGES / 2 + 1, 1};
    image.initial = image.final = ImageLayout::GENERAL;
    image.initializedRegions    = {{0, 0, {{0, 0}, {1, image.descriptor.extent.height}}}};
    FrameGraphData data;
    data.images.push_back(image);
    image.image = ImagePtr(new Image());
    data.images.push_back(image);
    auto result = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameError>(result));
    EXPECT_EQ(std::get<FrameError>(result).message, "Initial image content range budget exceeded");
    for (auto &input : data.images)
        --input.initializedRegions.front().rectangle.extent.height;
    EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    image.descriptor.extent.height                           = UINT32_MAX;
    image.initializedRegions.front().rectangle.extent.height = UINT32_MAX;
    EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveInitializedImageContents(image)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveInitializedImageContents(image, UINT32_MAX)));
    image.initializedRegions.front().rectangle.extent.width = 2;
    const auto contiguous                                   = ResolveInitializedImageContents(image, 1);
    ASSERT_TRUE(std::holds_alternative<ResolvedImageContents>(contiguous));
    EXPECT_EQ(std::get<ResolvedImageContents>(contiguous).ranges, (std::vector<ImageContentSpan>{{0, uint64_t(UINT32_MAX) * 2}}));
}

TEST_P(ImageRectangle, CompletedRectanglesContinueAcrossGraphsWithoutDefiningGaps)
{
    for (uint32_t workers : {1u, 3u}) {
        info.parallelNum = workers;
        ResetContext();
        auto image         = ImageOf();
        image.final        = ImageLayout::GENERAL;
        auto  sourceBuffer = BufferOf(true), outputBuffer = BufferOf(false);
        auto *bytes = sourceBuffer->Map();
        ASSERT_TRUE(bytes);
        std::fill_n(bytes, 16, uint8_t{31});
        std::fill_n(bytes + 16, 16, uint8_t{77});
        sourceBuffer->UnMap();
        auto left              = Region(1, 1);
        left.imageExtent.width = 2;
        FrameGraphBuilder first;
        const auto        source = Import(first, sourceBuffer, true);
        const auto        target = Import(first, image);
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(first.Copy({source, target, left, ImageCopyDirection::BUFFER_TO_IMAGE, true})));
        ASSERT_TRUE(Execute(Freeze(first)));

        image.initial            = ImageLayout::GENERAL;
        image.initializedRegions = {{1, 1, {{0, 0}, {2, 2}}}};
        FrameGraphBuilder second;
        const auto        nextSource = Import(second, sourceBuffer, true), output = Import(second, outputBuffer, false);
        const auto        nextTarget = Import(second, image);
        auto              right      = left;
        right.bufferOffset           = 16;
        right.imageOffset.x          = 2;
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(second.Copy({output, nextTarget, left, ImageCopyDirection::IMAGE_TO_BUFFER})));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(second.Copy({nextSource, nextTarget, right})));
        ASSERT_TRUE(
            std::holds_alternative<FrameOperationComplete>(second.Copy({output, nextTarget, Region(1, 1, 64), ImageCopyDirection::IMAGE_TO_BUFFER})));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(second.Export({output, 0, 16})));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(second.Export({output, 64, 32})));
        const auto graph = Freeze(second);
        ASSERT_TRUE(graph);
        const auto lease = Begin();
        for (uint32_t fault = 0; fault < 4; ++fault) {
            auto invalid = graph->GetData();
            if (fault == 0)
                invalid.images.front().initializedRegions.clear();
            else if (fault == 1)
                invalid.images.front().initializedRegions.front().layer = 0;
            else if (fault == 2)
                std::get<BufferImageCopyPass>(invalid.passes.front()).region.imageExtent.width = 4;
            else
                invalid.imageExports.push_back(nextTarget);
            const auto rejected = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(invalid)));
            EXPECT_TRUE(std::holds_alternative<FrameError>(rejected)) << fault;
        }
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        ASSERT_TRUE(Execute(graph, [&](const BackendCompiledFrame &plan) {
            if (GetParam() == 2)
                for (const auto &task : static_cast<const D3D12CompiledFrame &>(plan).encoder->tasks)
                    if (std::holds_alternative<D3D12ImageCopyStep>(task.operation))
                        EXPECT_FALSE(task.initializeImage);
        }));
        bytes = outputBuffer->Map();
        ASSERT_TRUE(bytes);
        for (uint32_t i = 0; i < 16; ++i)
            EXPECT_EQ(bytes[i], 31);
        for (uint32_t i = 0; i < 32; ++i)
            EXPECT_EQ(bytes[64 + i], i % 16 < 8 ? 31 : 77);
        outputBuffer->UnMap();

        image.initializedRegions.push_back({1, 1, {{2, 0}, {2, 2}}});
        FrameGraphBuilder third;
        const auto        finalTarget = Import(third, image);
        image.initializedRegions.clear();
        image.initializedRanges = {{1, 1, 1, 1, AspectFlagBit::COLOR_BIT}};
        EXPECT_EQ(Import(third, image), finalTarget);
        const auto finalOutput = Import(third, outputBuffer, false);
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(
            third.Copy({finalOutput, finalTarget, Region(1, 1), ImageCopyDirection::IMAGE_TO_BUFFER})));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(third.Export({finalOutput, 0, 32})));
        ASSERT_TRUE(Execute(Freeze(third)));
        bytes = outputBuffer->Map();
        ASSERT_TRUE(bytes);
        for (uint32_t i = 0; i < 32; ++i)
            EXPECT_EQ(bytes[i], i % 16 < 8 ? 31 : 77);
        outputBuffer->UnMap();
    }
}

TEST_P(ImageRectangle, RectangleUnionAllowsRasterLoadOnlyAfterAllTexelsAreDefined)
{
    auto desc      = ImageOf().descriptor;
    desc.extent    = {4, 2, 1};
    desc.mipLevels = desc.arrayLayers = 1;
    FrameGraphImage image;
    image.descriptor = desc;
    image.image      = ImagePtr(device->CreateImage(desc));
    ASSERT_TRUE(image.image);
    image.final        = ImageLayout::GENERAL;
    auto  sourceBuffer = BufferOf(true), outputBuffer = BufferOf(false);
    auto *bytes = sourceBuffer->Map();
    ASSERT_TRUE(bytes);
    std::fill_n(bytes, 32, uint8_t{117});
    sourceBuffer->UnMap();
    FrameGraphBuilder first;
    const auto        source = Import(first, sourceBuffer, true);
    const auto        target = Import(first, image);
    auto              left   = Region(0, 0);
    left.imageExtent         = {2, 2, 1};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(first.Copy({source, target, left, ImageCopyDirection::BUFFER_TO_IMAGE, true})));
    ASSERT_TRUE(Execute(Freeze(first)));
    image.initial            = ImageLayout::GENERAL;
    image.initializedRegions = {{0, 0, {{0, 0}, {2, 2}}}};
    FrameGraphBuilder second;
    const auto        nextSource = Import(second, sourceBuffer, true), output = Import(second, outputBuffer, false);
    const auto        nextTarget = Import(second, image);
    auto              right      = left;
    right.imageOffset.x          = 2;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(second.Copy({nextSource, nextTarget, right})));
    FrameRasterPass raster;
    raster.color = nextTarget;
    raster.load  = LoadOp::LOAD;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(second.Raster(raster)));
    auto full              = left;
    full.imageExtent.width = 4;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(second.Copy({output, nextTarget, full, ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(second.Export({output, 0, 32})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(second.Export(nextTarget)));
    const auto graph = Freeze(second);
    ASSERT_TRUE(graph);
    auto gap = graph->GetData();
    gap.passes.erase(gap.passes.begin());
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(graph->GetId(), std::move(gap)))));
    ASSERT_TRUE(Execute(graph));
    bytes = outputBuffer->Map();
    ASSERT_TRUE(bytes);
    for (uint32_t i = 0; i < 32; ++i)
        EXPECT_EQ(bytes[i], 117);
    outputBuffer->UnMap();
}

TEST(FrameGraphImports, ImageBoundaryTablesCompareSemanticLayoutsAndRequireExactCoverage)
{
    FrameGraphImage image;
    image.image                  = ImagePtr(new Image());
    image.descriptor.extent      = {8, 4, 1};
    image.descriptor.mipLevels   = 3;
    image.descriptor.arrayLayers = 2;
    image.descriptor.format      = PixelFormat::RGBA8_UNORM;
    image.initial = image.final = ImageLayout::GENERAL;
    FrameGraphBuilder builder;
    const auto        first = builder.Import(image);
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(first));
    image.initial         = ImageBoundaryState{std::vector<ImageLayout>(6, ImageLayout::GENERAL)};
    image.final           = image.initial;
    const auto equivalent = builder.Import(image);
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(equivalent));
    EXPECT_EQ(std::get<GraphImageHandle>(first), std::get<GraphImageHandle>(equivalent));
    std::get<std::vector<ImageLayout>>(image.final.layouts)[5] = ImageLayout::TRANSFER_DST;
    EXPECT_TRUE(std::holds_alternative<FrameError>(builder.Import(image)));
    for (uint32_t fault = 0; fault < 6; ++fault) {
        auto  invalid = image;
        auto &initial = std::get<std::vector<ImageLayout>>(invalid.initial.layouts);
        auto &final   = std::get<std::vector<ImageLayout>>(invalid.final.layouts);
        switch (fault) {
        case 0: initial.clear(); break;
        case 1: initial.pop_back(); break;
        case 2: initial.push_back(ImageLayout::GENERAL); break;
        case 3: initial[5] = static_cast<ImageLayout>(UINT32_MAX); break;
        case 4: final[5] = ImageLayout::UNDEFINED; break;
        case 5: final[5] = static_cast<ImageLayout>(UINT32_MAX); break;
        }
        FrameGraphBuilder rejected;
        EXPECT_TRUE(std::holds_alternative<FrameError>(rejected.Import(invalid))) << fault;
        FrameGraphData data;
        data.images.push_back(invalid);
        EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, std::move(data))))) << fault;
    }
    EXPECT_TRUE(ValidateImageBoundaryState(image.initial, MAX_FRAME_IMAGE_SUBRESOURCES + uint64_t{1}));
    EXPECT_TRUE(ValidateImageBoundaryState(ImageLayout::GENERAL, uint64_t{UINT32_MAX} * UINT32_MAX));
}

TEST(FrameGraphImports, MixedUndefinedLayoutsOnlyRejectContentsOnUndefinedSubresources)
{
    FrameGraphImage image;
    image.image                  = ImagePtr(new Image());
    image.descriptor.extent      = {4, 4, 1};
    image.descriptor.mipLevels   = 2;
    image.descriptor.arrayLayers = 2;
    image.descriptor.format      = PixelFormat::RGBA8_UNORM;
    image.initial            = ImageBoundaryState{{ImageLayout::UNDEFINED, ImageLayout::GENERAL, ImageLayout::UNDEFINED, ImageLayout::TRANSFER_SRC}};
    image.final              = ImageLayout::GENERAL;
    image.initializedRanges  = {{1, 1, 0, 1, AspectFlagBit::COLOR_BIT}};
    image.initializedRegions = {{1, 1, {{0, 0}, {1, 1}}}};
    EXPECT_TRUE(std::holds_alternative<ResolvedImageContents>(ResolveInitializedImageContents(image)));
    FrameGraphData data;
    data.images.push_back(image);
    EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    for (uint32_t fault = 0; fault < 3; ++fault) {
        auto invalid = image;
        if (fault == 0)
            invalid.initializedRanges.front().baseLevel = 0;
        if (fault == 1)
            invalid.initializedRegions.front().mip = 0;
        if (fault == 2) {
            invalid.initializedRanges.clear();
            invalid.initializedRegions.clear();
            invalid.initialized = true;
        }
        EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveInitializedImageContents(invalid))) << fault;
    }
    data.imageExports.push_back({1, 0});
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    data.imageExports.clear();
    data.images.front().image     = {};
    data.images.front().transient = true;
    data.images.front().initializedRanges.clear();
    data.images.front().initializedRegions.clear();
    data.images.front().descriptor.usage = ImageUsageFlagBit::TRANSFER_DST;
    data.images.front().initial          = ImageBoundaryState{std::vector<ImageLayout>(4, ImageLayout::UNDEFINED)};
    data.images.front().final            = ImageBoundaryState{std::vector<ImageLayout>(4, ImageLayout::GENERAL)};
    EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    std::get<std::vector<ImageLayout>>(data.images.front().final.layouts)[3] = ImageLayout::TRANSFER_DST;
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
}

namespace {
    class ImageBoundary : public ImageSubresource {};
} // namespace

TEST_P(ImageBoundary, MixedLayoutsSurviveBoundaryOnlyExportAndAllSubresourceReadback)
{
    for (const uint32_t workers : {1u, 3u}) {
        info.parallelNum = workers;
        ResetContext();
        auto                     image = ImageOf();
        const ImageBoundaryState first{{ImageLayout::GENERAL, ImageLayout::TRANSFER_SRC, ImageLayout::TRANSFER_DST, ImageLayout::GENERAL,
                                        ImageLayout::TRANSFER_DST, ImageLayout::GENERAL, ImageLayout::TRANSFER_SRC, ImageLayout::TRANSFER_DST}};
        const ImageBoundaryState second{{ImageLayout::TRANSFER_SRC, ImageLayout::TRANSFER_DST, ImageLayout::GENERAL, ImageLayout::TRANSFER_DST,
                                         ImageLayout::GENERAL, ImageLayout::TRANSFER_SRC, ImageLayout::TRANSFER_DST, ImageLayout::GENERAL}};
        image.final = first;
        ASSERT_TRUE(
            std::holds_alternative<FrameOperationComplete>(context->TrackResource(FrameResource{image.image.GetOwned()}, ImageLayout::UNDEFINED)));
        auto                 upload = BufferOf(true), output = BufferOf(false);
        std::vector<uint8_t> expected(448);
        for (uint32_t i = 0; i < expected.size(); ++i)
            expected[i] = static_cast<uint8_t>(i * 19 + 7);
        auto *bytes = upload->Map();
        ASSERT_TRUE(bytes);
        std::copy(expected.begin(), expected.end(), bytes);
        upload->UnMap();
        FrameGraphBuilder producer;
        const auto        source = Import(producer, upload, true);
        const auto        handle = Import(producer, image);
        uint64_t          offset = 0;
        for (uint32_t layer = 0; layer < 2; ++layer)
            for (uint32_t mip = 0; mip < 4; ++mip) {
                const auto region = Region(mip, layer, offset);
                ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(
                    producer.Copy({source, handle, region, ImageCopyDirection::BUFFER_TO_IMAGE, true})));
                offset += uint64_t(region.imageExtent.width) * region.imageExtent.height * 4;
            }
        ASSERT_EQ(offset, expected.size());
        ASSERT_TRUE(Execute(Freeze(producer)));
        const auto snapshot = std::get<FrameResourceStateSnapshot>(context->GetResourceState(FrameResource{image.image.GetOwned()}));
        EXPECT_EQ(snapshot.accepted.state, FrameResourceBoundaryState{first});
        EXPECT_EQ(snapshot.completed.state, snapshot.accepted.state);
        image.initial     = std::get<ImageBoundaryState>(snapshot.accepted.state);
        image.final       = second;
        image.initialized = true;
        FrameGraphBuilder boundary;
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(boundary.Export(Import(boundary, image))));
        std::unique_ptr<ImageStateProbe> probe;
        if (GetParam() != 2)
            probe = std::make_unique<ImageStateProbe>(*static_cast<VulkanDevice *>(device),
                                                      static_cast<VulkanImage *>(image.image.Get())->GetNativeHandle());
        ASSERT_TRUE(Execute(Freeze(boundary), [&](const BackendCompiledFrame &plan) {
            if (GetParam() != 2)
                return;
            const auto &tasks = static_cast<const D3D12CompiledFrame &>(plan).encoder->tasks;
            ASSERT_EQ(tasks.size(), 1u);
            const auto nativeState = [](ImageLayout layout) {
                return layout == ImageLayout::TRANSFER_SRC   ? D3D12_RESOURCE_STATE_COPY_SOURCE
                       : layout == ImageLayout::TRANSFER_DST ? D3D12_RESOURCE_STATE_COPY_DEST
                                                             : D3D12_RESOURCE_STATE_COMMON;
            };
            std::array<bool, 8> seen{};
            for (const auto &barrier : tasks.back().after) {
                const auto sub = barrier.Transition.Subresource;
                ASSERT_LT(sub, seen.size());
                EXPECT_EQ(barrier.Transition.StateBefore, nativeState(first.At(sub)));
                EXPECT_EQ(barrier.Transition.StateAfter, nativeState(second.At(sub)));
                seen[sub] = true;
            }
            EXPECT_TRUE(std::all_of(seen.begin(), seen.end(), [](bool value) { return value; }));
        }));
        if (probe) {
            const auto nativeLayout = [](ImageLayout layout) {
                return layout == ImageLayout::TRANSFER_SRC   ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                       : layout == ImageLayout::TRANSFER_DST ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                                                             : VK_IMAGE_LAYOUT_GENERAL;
            };
            std::array<bool, 8> seen{};
            for (const auto &barrier : probe->barriers) {
                const auto &range = barrier.subresourceRange;
                for (uint32_t layer = range.baseArrayLayer; layer < range.baseArrayLayer + range.layerCount; ++layer)
                    for (uint32_t mip = range.baseMipLevel; mip < range.baseMipLevel + range.levelCount; ++mip) {
                        const auto sub = layer * 4 + mip;
                        ASSERT_LT(sub, seen.size());
                        EXPECT_EQ(barrier.oldLayout, nativeLayout(first.At(sub)));
                        EXPECT_EQ(barrier.newLayout, nativeLayout(second.At(sub)));
                        seen[sub] = true;
                    }
            }
            EXPECT_TRUE(std::all_of(seen.begin(), seen.end(), [](bool value) { return value; }));
            probe.reset();
        }
        const auto changed = std::get<FrameResourceStateSnapshot>(context->GetResourceState(FrameResource{image.image.GetOwned()}));
        EXPECT_GT(changed.accepted.submission.serial, snapshot.accepted.submission.serial);
        EXPECT_EQ(changed.completed.state, FrameResourceBoundaryState{second});
        image.initial = std::get<ImageBoundaryState>(changed.accepted.state);
        image.final   = ImageLayout::GENERAL;
        FrameGraphBuilder consumer;
        const auto        target = Import(consumer, output, false);
        const auto        read   = Import(consumer, image);
        offset                   = 0;
        for (uint32_t layer = 0; layer < 2; ++layer)
            for (uint32_t mip = 0; mip < 4; ++mip) {
                const auto region = Region(mip, layer, offset);
                ASSERT_TRUE(
                    std::holds_alternative<FrameOperationComplete>(consumer.Copy({target, read, region, ImageCopyDirection::IMAGE_TO_BUFFER})));
                offset += uint64_t(region.imageExtent.width) * region.imageExtent.height * 4;
            }
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(consumer.Export({target, 0, offset})));
        ASSERT_TRUE(Execute(Freeze(consumer)));
        bytes = output->Map();
        ASSERT_TRUE(bytes);
        EXPECT_TRUE(std::equal(expected.begin(), expected.end(), bytes));
        output->UnMap();
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->UntrackResource(FrameResource{image.image.GetOwned()})));
    }
}

TEST_P(ImageBoundary, InvalidUnusedMipBoundariesDoNotBorrowOrPoisonTheLease)
{
    auto              image = ImageOf();
    FrameGraphBuilder builder;
    const auto        source = Import(builder, BufferOf(true), true);
    const auto        handle = Import(builder, image);
    ASSERT_TRUE(
        std::holds_alternative<FrameOperationComplete>(builder.Copy({source, handle, Region(1, 0), ImageCopyDirection::BUFFER_TO_IMAGE, true})));
    const auto graph = Freeze(builder);
    const auto lease = Begin();
    for (uint32_t fault = 0; fault < 5; ++fault) {
        auto  data     = graph->GetData();
        auto &target   = data.images.front();
        target.initial = ImageBoundaryState{std::vector<ImageLayout>(8, ImageLayout::UNDEFINED)};
        target.final   = ImageBoundaryState{std::vector<ImageLayout>(8, ImageLayout::GENERAL)};
        auto &initial  = std::get<std::vector<ImageLayout>>(target.initial.layouts);
        auto &final    = std::get<std::vector<ImageLayout>>(target.final.layouts);
        if (fault == 0)
            initial.pop_back();
        if (fault == 1)
            initial[7] = static_cast<ImageLayout>(UINT32_MAX);
        if (fault == 2)
            final[7] = ImageLayout::UNDEFINED;
        if (fault == 3)
            final[7] = ImageLayout::PRESENT;
        if (fault == 4)
            final[7] = ImageLayout::SHADER_READ_ONLY;
        const auto failed = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
        ASSERT_TRUE(std::holds_alternative<FrameError>(failed)) << fault;
        EXPECT_EQ(std::get<FrameError>(failed).code, fault == 3 ? FrameErrorCode::UNSUPPORTED : FrameErrorCode::INVALID_ARGUMENT);
    }
    auto good = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(good));
    std::get<std::unique_ptr<BackendCompiledFrame>>(good).reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

INSTANTIATE_TEST_SUITE_P(Backends, ImageBoundary, testing::Values(0u, 1u, 2u));

namespace {
    class AttachmentViews : public ImageSubresource {};
} // namespace

TEST_P(AttachmentViews, ClearLoadAndReadbackEachMipLayerWithViewAwareScopeMerging)
{
    for (const auto workers : {1u, 3u}) {
        info.parallelNum = workers;
        ResetContext();
        FrameGraphBuilder builder;
        const auto        storage  = ImageOf();
        const auto        image    = Import(builder, storage);
        const auto        readback = BufferOf(false);
        const auto        output   = Import(builder, readback, false);
        for (uint32_t layer = 0; layer < 2; ++layer)
            for (uint32_t mip = 0; mip < 4; ++mip) {
                FrameRasterPass pass;
                pass.color     = image;
                pass.colorView = {mip, layer};
                pass.clear     = {float(layer), float(mip & 1u), float((mip >> 1) & 1u), 1.f};
                ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
                pass.load = LoadOp::LOAD;
                ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
            }
        uint64_t offset = 0;
        for (uint32_t layer = 0; layer < 2; ++layer)
            for (uint32_t mip = 0; mip < 4; ++mip) {
                const auto region = Region(mip, layer, offset);
                ASSERT_TRUE(
                    std::holds_alternative<FrameOperationComplete>(builder.Copy({output, image, region, ImageCopyDirection::IMAGE_TO_BUFFER})));
                offset += uint64_t(region.imageExtent.width) * region.imageExtent.height * 4;
            }
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({output, 0, offset})));
        ASSERT_TRUE(Execute(Freeze(builder), [&](const BackendCompiledFrame &compiled) {
            if (GetParam() == 2) {
                const auto &plan = static_cast<const D3D12CompiledFrame &>(compiled);
                EXPECT_EQ(plan.encoder->scopes.size(), 16u);
            } else {
                const auto &plan = static_cast<const VulkanCompiledFrame &>(compiled);
                ASSERT_EQ(plan.rasterPasses.size(), 16u);
                for (uint32_t i = 0; i < 16; ++i)
                    EXPECT_EQ(plan.rasterPasses[i].nativeScope, GetParam() == 1 ? i / 2 : i);
            }
        }));
        auto *bytes = readback->Map();
        ASSERT_TRUE(bytes);
        offset = 0;
        for (uint32_t layer = 0; layer < 2; ++layer)
            for (uint32_t mip = 0; mip < 4; ++mip) {
                const auto region = Region(mip, layer);
                for (uint32_t pixel = 0; pixel < region.imageExtent.width * region.imageExtent.height; ++pixel) {
                    EXPECT_EQ(bytes[offset++], layer * 255);
                    EXPECT_EQ(bytes[offset++], (mip & 1u) * 255);
                    EXPECT_EQ(bytes[offset++], ((mip >> 1) & 1u) * 255);
                    EXPECT_EQ(bytes[offset++], 255);
                }
            }
        readback->UnMap();
    }
}

TEST_P(AttachmentViews, RasterWritesPreserveOtherCellsAndEarlierVersions)
{
    FrameGraphBuilder builder;
    const auto        storage  = ImageOf();
    const auto        image    = Import(builder, storage);
    const auto        upload   = BufferOf(true);
    const auto        readback = BufferOf(false);
    auto             *bytes    = upload->Map();
    ASSERT_TRUE(bytes);
    std::fill_n(bytes, 1024, uint8_t{73});
    upload->UnMap();
    const auto input  = Import(builder, upload, true);
    const auto output = Import(builder, readback, false);
    for (uint32_t layer = 0; layer < 2; ++layer)
        for (uint32_t mip = 0; mip < 4; ++mip)
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({input, image, Region(mip, layer)})));
    ASSERT_TRUE(
        std::holds_alternative<FrameOperationComplete>(builder.Copy({output, image, Region(1, 1, 512), ImageCopyDirection::IMAGE_TO_BUFFER})));
    FrameRasterPass pass;
    pass.color     = image;
    pass.colorView = {1, 1};
    pass.clear     = {1.f, 0.f, 0.f, 1.f};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
    pass.load = LoadOp::LOAD;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
    uint64_t offset = 0;
    for (uint32_t layer = 0; layer < 2; ++layer)
        for (uint32_t mip = 0; mip < 4; ++mip) {
            const auto region = Region(mip, layer, offset);
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, image, region, ImageCopyDirection::IMAGE_TO_BUFFER})));
            offset += uint64_t(region.imageExtent.width) * region.imageExtent.height * 4;
        }
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({output, 0, offset})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({output, 512, 32})));
    ASSERT_TRUE(Execute(Freeze(builder)));
    bytes = readback->Map();
    ASSERT_TRUE(bytes);
    offset = 0;
    for (uint32_t layer = 0; layer < 2; ++layer)
        for (uint32_t mip = 0; mip < 4; ++mip) {
            const auto region = Region(mip, layer);
            for (uint32_t pixel = 0; pixel < region.imageExtent.width * region.imageExtent.height; ++pixel)
                for (uint32_t channel = 0; channel < 4; ++channel)
                    EXPECT_EQ(bytes[offset++], mip == 1 && layer == 1 ? (channel == 0 || channel == 3 ? 255 : 0) : 73);
        }
    for (uint32_t i = 0; i < 32; ++i)
        EXPECT_EQ(bytes[512 + i], 73);
    readback->UnMap();
}

TEST_P(AttachmentViews, ContentCullingLoadsAndDiscardAreScopedToSelectedCell)
{
    FrameGraphBuilder builder;
    const auto        image  = Import(builder, ImageOf());
    const auto        output = Import(builder, BufferOf(false), false);
    FrameRasterPass   pass;
    pass.color     = image;
    pass.colorView = {1, 1};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
    pass.colorView = {2, 0};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, image, Region(1, 1), ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({output, 0, 32})));
    const auto graph    = Freeze(builder);
    auto       analyzed = AnalyzeFrameGraph(*graph);
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analyzed));
    EXPECT_EQ(std::get<FrameGraphAnalysis>(analyzed).livePasses, (std::vector<uint32_t>{0, 2}));
    for (uint32_t invalid = 0; invalid < 4; ++invalid) {
        auto  data  = graph->GetData();
        auto &first = std::get<FrameRasterPass>(data.passes[0]);
        if (invalid == 0)
            first.load = LoadOp::LOAD;
        else if (invalid == 1)
            first.store = StoreOp::DONT_CARE;
        else if (invalid == 2)
            std::get<BufferImageCopyPass>(data.passes[2]).region.subRange.level = 0;
        else
            data.imageExports.push_back(image);
        EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(graph->GetId(), std::move(data))))) << invalid;
    }
    auto  data          = graph->GetData();
    auto &neighbor      = std::get<FrameRasterPass>(data.passes[1]);
    neighbor.sideEffect = true;
    neighbor.store      = StoreOp::DONT_CARE;
    EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(FrozenFrameGraph(graph->GetId(), std::move(data)))));
}

INSTANTIATE_TEST_SUITE_P(Backends, AttachmentViews, testing::Values(0u, 1u, 2u));

TEST_P(AttachmentViews, SwitchingDepthViewSplitsScopesAndKeepsEarlierDepthContents)
{
    FrameGraphBuilder builder;
    const auto        color = Import(builder, ImageOf());
    FrameGraphImage   depth;
    depth.descriptor.format      = PixelFormat::D32;
    depth.descriptor.extent      = {9, 5, 1};
    depth.descriptor.mipLevels   = 4;
    depth.descriptor.arrayLayers = 2;
    depth.descriptor.usage       = ImageUsageFlagBit::DEPTH_STENCIL;
    depth.final                  = ImageLayout::DEPTH_STENCIL_ATTACHMENT;
    depth.image                  = ImagePtr(device->CreateImage(depth.descriptor));
    ASSERT_TRUE(depth.image);
    const auto      depthImage = Import(builder, depth);
    FrameRasterPass pass;
    pass.color       = color;
    pass.colorView   = {1, 1};
    pass.depth       = RasterDepthAttachment{depthImage};
    pass.depth->view = {1, 0};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
    pass.load              = LoadOp::LOAD;
    pass.depth->view.layer = 1;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
    pass.depth->view.layer = 0;
    pass.depth->load       = LoadOp::LOAD;
    pass.sideEffect        = true;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
    ASSERT_TRUE(Execute(Freeze(builder), [&](const BackendCompiledFrame &compiled) {
        if (GetParam() == 2) {
            EXPECT_EQ(static_cast<const D3D12CompiledFrame &>(compiled).encoder->scopes.size(), 3u);
        } else {
            const auto &plan = static_cast<const VulkanCompiledFrame &>(compiled);
            ASSERT_EQ(plan.rasterPasses.size(), 3u);
            for (uint32_t i = 0; i < 3; ++i)
                EXPECT_EQ(plan.rasterPasses[i].nativeScope, i);
        }
    }));
}

namespace {
    struct AttachmentViewProbe {
        explicit AttachmentViewProbe(VulkanDevice &device, VkImage image = VK_NULL_HANDLE)
            : fn(const_cast<VulkanDeviceFunctions &>(device.GetDeviceFn())), saved(fn), image(image)
        {
            active                = this;
            fn.vkCreateImageView  = Create;
            fn.vkDestroyImageView = Destroy;
        }
        ~AttachmentViewProbe()
        {
            fn     = saved;
            active = nullptr;
        }
        static VkResult VKAPI_PTR Create(VkDevice device, const VkImageViewCreateInfo *info, const VkAllocationCallbacks *allocator, VkImageView *out)
        {
            if (active->image && active->image != info->image)
                return active->saved.vkCreateImageView(device, info, allocator, out);
            ++active->attempts;
            if (active->frozen)
                ++active->lateCreates;
            if (active->attempts == active->failAt)
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            const auto result = active->saved.vkCreateImageView(device, info, allocator, out);
            if (result == VK_SUCCESS)
                active->live.push_back(*out);
            return result;
        }
        static void VKAPI_PTR Destroy(VkDevice device, VkImageView view, const VkAllocationCallbacks *allocator)
        {
            if (std::erase(active->live, view))
                ++active->destroyed;
            active->saved.vkDestroyImageView(device, view, allocator);
        }
        inline static AttachmentViewProbe *active = nullptr;
        VkImage                            image  = VK_NULL_HANDLE;
        VulkanDeviceFunctions             &fn;
        VulkanDeviceFunctions              saved;
        std::vector<VkImageView>           live;
        uint32_t                           attempts = 0, failAt = 0, destroyed = 0, lateCreates = 0;
        bool                               frozen = false;
    };
    class VulkanAttachmentViews : public ImageSubresource {};
} // namespace

TEST_P(VulkanAttachmentViews, ViewsRollbackShareAndStayAliveUntilDrainOrRetirement)
{
    for (uint32_t outcome = 0; outcome < 3; ++outcome) {
        ResetContext();
        FrameGraphBuilder builder;
        const auto        color = Import(builder, ImageOf());
        FrameGraphImage   depth;
        depth.descriptor.format      = PixelFormat::D32;
        depth.descriptor.extent      = {9, 5, 1};
        depth.descriptor.mipLevels   = 4;
        depth.descriptor.arrayLayers = 2;
        depth.descriptor.usage       = ImageUsageFlagBit::DEPTH_STENCIL;
        depth.final                  = ImageLayout::DEPTH_STENCIL_ATTACHMENT;
        depth.image                  = ImagePtr(device->CreateImage(depth.descriptor));
        ASSERT_TRUE(depth.image);
        FrameRasterPass pass;
        pass.color       = color;
        pass.colorView   = {1, 1};
        pass.depth       = RasterDepthAttachment{Import(builder, depth)};
        pass.depth->view = {1, 1};
        pass.sideEffect  = true;
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
        pass.load        = LoadOp::LOAD;
        pass.depth->load = LoadOp::LOAD;
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
        const auto          graph = Freeze(builder);
        const auto          lease = Begin();
        AttachmentViewProbe views(*static_cast<VulkanDevice *>(device));
        views.failAt = 2;
        auto failed  = context->Compile(lease, graph);
        ASSERT_TRUE(std::holds_alternative<FrameError>(failed));
        EXPECT_EQ(std::get<FrameError>(failed).code, FrameErrorCode::COMPILE_FAILED);
        EXPECT_EQ(views.attempts, 2u);
        EXPECT_EQ(views.destroyed, 1u);
        EXPECT_TRUE(views.live.empty());
        views.failAt   = 0;
        auto  compiled = context->Compile(lease, graph);
        auto *plan     = std::get_if<std::unique_ptr<BackendCompiledFrame>>(&compiled);
        ASSERT_TRUE(plan);
        EXPECT_EQ(views.live.size(), 2u);
        EXPECT_EQ(views.attempts, 4u);
        views.frozen = true;
        if (outcome == 0) {
            plan->reset();
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        } else {
            auto dispatched = context->Dispatch(*plan);
            ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
            const auto ticket = std::get<EncodingTicket>(dispatched);
            if (outcome == 1)
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->Cancel(ticket)));
            auto       collected = context->TryCollect(ticket);
            const auto deadline  = std::chrono::steady_clock::now() + 5s;
            while (std::holds_alternative<FramePending>(collected) && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(1ms);
                collected = context->TryCollect(ticket);
            }
            if (outcome == 1) {
                EXPECT_TRUE(std::holds_alternative<EncodingCancelled>(collected));
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
            } else {
                auto *recording = std::get_if<std::unique_ptr<RecordedFrame>>(&collected);
                ASSERT_TRUE(recording);
                ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(*recording)));
                device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
                EXPECT_EQ(views.live.size(), 2u);
                std::array<FrameCompletion, 1> completed;
                ASSERT_EQ(context->PollCompleted(completed), 1u);
                EXPECT_EQ(completed[0].status, FrameCompletionStatus::COMPLETE);
            }
        }
        EXPECT_TRUE(views.live.empty());
        EXPECT_EQ(views.destroyed, 3u);
        EXPECT_EQ(views.lateCreates, 0u);
    }
}

INSTANTIATE_TEST_SUITE_P(VulkanModes, VulkanAttachmentViews, testing::Values(0u, 1u));

namespace {
    struct MultisampleSupportProbe {
        explicit MultisampleSupportProbe(Device &device)
        {
            active = this;
            if (device.GetAPI() == API::VULKAN) {
                vk      = &const_cast<VulkanInstanceFunctions &>(static_cast<VulkanDevice &>(device).GetVulkanInstance().GetInstanceFn());
                savedVk = vk->vkGetPhysicalDeviceImageFormatProperties;
                vk->vkGetPhysicalDeviceImageFormatProperties = QueryVk;
            } else {
                dx                     = &const_cast<D3D12Functions &>(static_cast<D3D12Device &>(device).GetFunctions());
                savedDx                = dx->multisampleQuality;
                dx->multisampleQuality = QueryDx;
            }
        }
        ~MultisampleSupportProbe()
        {
            if (vk)
                vk->vkGetPhysicalDeviceImageFormatProperties = savedVk;
            if (dx)
                dx->multisampleQuality = savedDx;
            active = nullptr;
        }
        static VkResult VKAPI_PTR QueryVk(VkPhysicalDevice         device,
                                          VkFormat                 format,
                                          VkImageType              type,
                                          VkImageTiling            tiling,
                                          VkImageUsageFlags        usage,
                                          VkImageCreateFlags       flags,
                                          VkImageFormatProperties *properties)
        {
            if (active->depthOnly && format != VK_FORMAT_D32_SFLOAT)
                return active->savedVk(device, format, type, tiling, usage, flags, properties);
            if (active->depthOnly)
                EXPECT_EQ(usage, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
            ++active->queries;
            if (active->fail)
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            const auto result        = active->savedVk(device, format, type, tiling, usage, flags, properties);
            properties->sampleCounts = VK_SAMPLE_COUNT_1_BIT;
            return result;
        }
        static HRESULT QueryDx(ID3D12Device *device, D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS *quality)
        {
            if (active->depthOnly && quality->Format != DXGI_FORMAT_D32_FLOAT)
                return active->savedDx(device, quality);
            ++active->queries;
            quality->NumQualityLevels = 0;
            return active->fail ? E_OUTOFMEMORY : S_OK;
        }
        inline static MultisampleSupportProbe       *active    = nullptr;
        VulkanInstanceFunctions                     *vk        = nullptr;
        D3D12Functions                              *dx        = nullptr;
        PFN_vkGetPhysicalDeviceImageFormatProperties savedVk   = nullptr;
        decltype(D3D12Functions::multisampleQuality) savedDx   = nullptr;
        bool                                         fail      = false;
        bool                                         depthOnly = false;
        uint32_t                                     queries   = 0;
    };
    class MultisampleResolve : public ImageSubresource {
    protected:
        Image::Descriptor Multisampled(SampleCount samples = SampleCount::X4)
        {
            Image::Descriptor desc;
            desc.format      = PixelFormat::RGBA8_UNORM;
            desc.extent      = {4, 2, 1};
            desc.arrayLayers = 2;
            desc.samples     = samples;
            desc.usage       = ImageUsageFlagBit::RENDER_TARGET;
            return desc;
        }
        FrameTransientPool &Pool()
        {
            return GetParam() == 2 ? static_cast<D3D12Device *>(device)->GetTransientPool() : static_cast<VulkanDevice *>(device)->GetTransientPool();
        }
        GraphImageHandle Source(FrameGraphBuilder &builder, SampleCount samples = SampleCount::X4)
        {
            auto created = builder.CreateTransient(Multisampled(samples));
            EXPECT_TRUE(std::holds_alternative<GraphImageHandle>(created));
            return std::get<GraphImageHandle>(created);
        }
        std::shared_ptr<const FrozenFrameGraph> ResolveGraph(StoreOp store = StoreOp::DONT_CARE)
        {
            FrameGraphBuilder builder;
            const auto        source = Source(builder);
            const auto        target = Import(builder, ImageOf());
            FrameRasterPass   pass;
            pass.color        = source;
            pass.colorView    = {0, 1};
            pass.store        = store;
            pass.colorResolve = RasterColorResolve{target, {1, 1}};
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
            const auto output = Import(builder, BufferOf(false), false);
            EXPECT_TRUE(
                std::holds_alternative<FrameOperationComplete>(builder.Copy({output, target, Region(1, 1), ImageCopyDirection::IMAGE_TO_BUFFER})));
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({output, 0, 32})));
            return Freeze(builder);
        }
    };
} // namespace

TEST_P(MultisampleResolve, ResolveDiscardedColorPreservesUnselectedTargetCells)
{
    for (auto samples : {SampleCount::X2, SampleCount::X4, SampleCount::X8})
        for (const auto workers : {1u, 3u}) {
            info.parallelNum = workers;
            ResetContext();
            FrameGraphBuilder builder;
            const auto        source   = Source(builder, samples);
            const auto        target   = Import(builder, ImageOf());
            const auto        upload   = BufferOf(true);
            const auto        readback = BufferOf(false);
            auto             *bytes    = upload->Map();
            ASSERT_TRUE(bytes);
            std::fill_n(bytes, 1024, uint8_t{73});
            upload->UnMap();
            const auto input  = Import(builder, upload, true);
            const auto output = Import(builder, readback, false);
            for (uint32_t layer = 0; layer < 2; ++layer)
                for (uint32_t mip = 0; mip < 4; ++mip)
                    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({input, target, Region(mip, layer)})));
            FrameRasterPass pass;
            pass.color        = source;
            pass.colorView    = {0, 1};
            pass.clear        = {1.f, 0.f, 0.f, 1.f};
            pass.store        = StoreOp::DONT_CARE;
            pass.colorResolve = RasterColorResolve{target, {1, 1}};
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
            uint64_t offset = 0;
            for (uint32_t layer = 0; layer < 2; ++layer)
                for (uint32_t mip = 0; mip < 4; ++mip) {
                    const auto region = Region(mip, layer, offset);
                    ASSERT_TRUE(
                        std::holds_alternative<FrameOperationComplete>(builder.Copy({output, target, region, ImageCopyDirection::IMAGE_TO_BUFFER})));
                    offset += uint64_t(region.imageExtent.width) * region.imageExtent.height * 4;
                }
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({output, 0, offset})));
            ASSERT_TRUE(Execute(Freeze(builder), [&](const BackendCompiledFrame &compiled) {
                if (GetParam() == 2) {
                    const auto &plan   = static_cast<const D3D12CompiledFrame &>(compiled);
                    const auto resolve = std::find_if(plan.encoder->tasks.begin(), plan.encoder->tasks.end(),
                                                      [](const auto &task) { return std::holds_alternative<D3D12ColorResolveStep>(task.operation); });
                    ASSERT_NE(resolve, plan.encoder->tasks.end());
                    const auto &step = std::get<D3D12ColorResolveStep>(resolve->operation);
                    EXPECT_EQ(step.sourceSubresource, 1u);
                    EXPECT_EQ(step.destinationSubresource, 5u);
                } else if (GetParam() == 0) {
                    const auto &plan = static_cast<const VulkanCompiledFrame &>(compiled);
                    ASSERT_EQ(plan.rasterPasses.size(), 1u);
                    EXPECT_EQ(std::get<VulkanRenderingInheritance>(plan.rasterPasses[0].inheritance).samples,
                              static_cast<VkSampleCountFlagBits>(samples));
                }
            }));
            bytes = readback->Map();
            ASSERT_TRUE(bytes);
            offset = 0;
            for (uint32_t layer = 0; layer < 2; ++layer)
                for (uint32_t mip = 0; mip < 4; ++mip) {
                    const auto region = Region(mip, layer);
                    for (uint32_t pixel = 0; pixel < region.imageExtent.width * region.imageExtent.height; ++pixel)
                        for (uint32_t channel = 0; channel < 4; ++channel)
                            EXPECT_EQ(bytes[offset++], mip == 1 && layer == 1 ? (channel == 0 || channel == 3 ? 255 : 0) : 73);
                }
            readback->UnMap();
        }
}

TEST_P(MultisampleResolve, InvalidDeclarationsAndContentDoNotAllocateOrConsumeLease)
{
    const auto graph = ResolveGraph();
    const auto lease = Begin();
    for (uint32_t fault = 0; fault < 13; ++fault) {
        auto  data = graph->GetData();
        auto &pass = std::get<FrameRasterPass>(data.passes[0]);
        switch (fault) {
        case 0: data.images[0].descriptor.samples = SampleCount::X1; break;
        case 1: data.images[0].descriptor.samples = static_cast<SampleCount>(3); break;
        case 2: data.images[0].descriptor.mipLevels = 2; break;
        case 3: pass.colorView.layer = 2; break;
        case 4: pass.colorResolve->view.mip = 4; break;
        case 5: pass.colorResolve->view.mip = 0; break;
        case 6: pass.colorResolve->image = pass.color; break;
        case 7: pass.load = LoadOp::DONT_CARE; break;
        case 8: pass.load = LoadOp::LOAD; break;
        case 9: data.imageExports.push_back(pass.color); break;
        case 10: data.imageExports.push_back(pass.colorResolve->image); break;
        case 11: std::get<BufferImageCopyPass>(data.passes[1]).image = pass.color; break;
        case 12: data.images[0].descriptor.format = PixelFormat::D32; break;
        }
        const auto result = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
        EXPECT_TRUE(std::holds_alternative<FrameError>(result)) << fault;
        EXPECT_EQ(Pool().GetStats().allocations, 0u);
    }
    auto valid = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(valid));
    std::get<std::unique_ptr<BackendCompiledFrame>>(valid).reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(MultisampleResolve, CapabilityRejectionAndNativeFailurePrecedeAllocationAndAllowRetry)
{
    const auto graph = ResolveGraph();
    const auto lease = Begin();
    {
        MultisampleSupportProbe probe(*device);
        for (const bool fail : {false, true}) {
            probe.fail  = fail;
            auto result = context->Compile(lease, graph);
            ASSERT_TRUE(std::holds_alternative<FrameError>(result));
            EXPECT_EQ(std::get<FrameError>(result).code, fail ? FrameErrorCode::COMPILE_FAILED : FrameErrorCode::UNSUPPORTED);
            if (fail)
                EXPECT_EQ(std::get<FrameError>(result).cause.code, GpuErrorCode::OUT_OF_MEMORY);
            EXPECT_EQ(Pool().GetStats().allocations, 0u);
        }
        EXPECT_EQ(probe.queries, 2u);
        auto dead = graph->GetData();
        dead.exports.clear();
        auto noWork = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(dead)));
        EXPECT_TRUE(std::holds_alternative<FrameNoWork>(noWork));
        EXPECT_EQ(probe.queries, 2u);
    }
    auto valid = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(valid));
    std::get<std::unique_ptr<BackendCompiledFrame>>(valid).reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(MultisampleResolve, TransientBudgetIncludesSamplesAndBackingWaitsForBorrowers)
{
    const auto graph    = ResolveGraph();
    auto       analyzed = AnalyzeFrameGraph(*graph);
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analyzed));
    const auto        &analysis = std::get<FrameGraphAnalysis>(analyzed);
    const uint64_t     bytes    = 4 * 2 * 2 * 4 * 4;
    FrameTransientPool tooSmall(*device, {4, bytes - 1});
    EXPECT_TRUE(std::holds_alternative<FrameError>(tooSmall.Acquire(graph->GetData(), analysis, info.deviceEpoch)));
    EXPECT_EQ(tooSmall.GetStats().allocations, 0u);
    FrameTransientPool pool(*device, {4, bytes});
    auto               first = pool.Acquire(graph->GetData(), analysis, info.deviceEpoch);
    ASSERT_TRUE(std::holds_alternative<FrameResourceBindings>(first));
    EXPECT_EQ(pool.GetStats().bytes, bytes);
    EXPECT_EQ(pool.GetStats().borrowed, 1u);
    EXPECT_TRUE(std::holds_alternative<FrameError>(pool.Acquire(graph->GetData(), analysis, info.deviceEpoch)));
    first       = FrameError{};
    auto reused = pool.Acquire(graph->GetData(), analysis, info.deviceEpoch);
    ASSERT_TRUE(std::holds_alternative<FrameResourceBindings>(reused));
    EXPECT_EQ(pool.GetStats().allocations, 1u);
    EXPECT_EQ(pool.GetStats().reuses, 1u);
}

INSTANTIATE_TEST_SUITE_P(Backends, MultisampleResolve, testing::Values(0u, 1u, 2u));

TEST_P(MultisampleResolve, TransientResolveStorageRetiresAfterCancelDrainOrGpuCompletion)
{
    for (const bool cancel : {true, false}) {
        ResetContext();
        const auto graph    = ResolveGraph();
        const auto lease    = Begin();
        auto       compiled = context->Compile(lease, graph);
        auto      *plan     = std::get_if<std::unique_ptr<BackendCompiledFrame>>(&compiled);
        ASSERT_TRUE(plan);
        EXPECT_EQ(Pool().GetStats().borrowed, 1u);
        auto dispatched = context->Dispatch(*plan);
        ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
        const auto ticket = std::get<EncodingTicket>(dispatched);
        if (cancel)
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->Cancel(ticket)));
        auto       collected = context->TryCollect(ticket);
        const auto deadline  = std::chrono::steady_clock::now() + 5s;
        while (std::holds_alternative<FramePending>(collected) && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
            collected = context->TryCollect(ticket);
        }
        if (cancel) {
            EXPECT_TRUE(std::holds_alternative<EncodingCancelled>(collected));
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        } else {
            auto *recording = std::get_if<std::unique_ptr<RecordedFrame>>(&collected);
            ASSERT_TRUE(recording);
            ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(*recording)));
            device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
            EXPECT_EQ(Pool().GetStats().borrowed, 1u);
            std::array<FrameCompletion, 1> completed;
            ASSERT_EQ(context->PollCompleted(completed), 1u);
            EXPECT_EQ(completed[0].status, FrameCompletionStatus::COMPLETE);
        }
        EXPECT_EQ(Pool().GetStats().borrowed, 0u);
    }
}

namespace {
    class VulkanMultisampleResolve : public MultisampleResolve {};
} // namespace

TEST_P(VulkanMultisampleResolve, ResolveViewFailureRollsBackAndSuccessfulViewRetiresWithGpu)
{
    const auto          graph  = ResolveGraph();
    const auto          lease  = Begin();
    const auto         *target = static_cast<VulkanImage *>(graph->GetData().images[1].image.Get());
    AttachmentViewProbe views(*static_cast<VulkanDevice *>(device), target->GetNativeHandle());
    views.failAt = 1;
    auto failed  = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<FrameError>(failed));
    EXPECT_EQ(std::get<FrameError>(failed).code, FrameErrorCode::COMPILE_FAILED);
    EXPECT_EQ(std::get<FrameError>(failed).cause.code, GpuErrorCode::OUT_OF_MEMORY);
    EXPECT_EQ(Pool().GetStats().borrowed, 0u);
    EXPECT_TRUE(views.live.empty());
    views.failAt   = 0;
    auto  compiled = context->Compile(lease, graph);
    auto *plan     = std::get_if<std::unique_ptr<BackendCompiledFrame>>(&compiled);
    ASSERT_TRUE(plan);
    EXPECT_EQ(views.attempts, 2u);
    EXPECT_EQ(views.live.size(), 1u);
    views.frozen    = true;
    auto dispatched = context->Dispatch(*plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto       collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    const auto deadline  = std::chrono::steady_clock::now() + 5s;
    while (std::holds_alternative<FramePending>(collected) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
        collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    }
    auto *recording = std::get_if<std::unique_ptr<RecordedFrame>>(&collected);
    ASSERT_TRUE(recording);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(*recording)));
    device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
    EXPECT_EQ(views.live.size(), 1u);
    std::array<FrameCompletion, 1> completed;
    ASSERT_EQ(context->PollCompleted(completed), 1u);
    EXPECT_EQ(completed[0].status, FrameCompletionStatus::COMPLETE);
    EXPECT_TRUE(views.live.empty());
    EXPECT_EQ(views.destroyed, 1u);
    EXPECT_EQ(views.lateCreates, 0u);
    EXPECT_EQ(Pool().GetStats().borrowed, 0u);
}

INSTANTIATE_TEST_SUITE_P(VulkanModes, VulkanMultisampleResolve, testing::Values(0u, 1u));

namespace {
    class MultisampleDepth : public MultisampleResolve {
    protected:
        std::shared_ptr<const FrozenFrameGraph> DepthGraph()
        {
            const auto      graph = ResolveGraph(StoreOp::STORE);
            auto            data  = graph->GetData();
            FrameGraphImage depth;
            depth.descriptor        = Multisampled();
            depth.descriptor.format = PixelFormat::D32;
            depth.descriptor.usage  = ImageUsageFlagBit::DEPTH_STENCIL;
            depth.transient         = true;
            depth.final             = ImageLayout::GENERAL;
            const GraphImageHandle handle{graph->GetId(), static_cast<uint32_t>(data.images.size())};
            data.images.push_back(std::move(depth));
            auto &first       = std::get<FrameRasterPass>(data.passes[0]);
            first.depth       = RasterDepthAttachment{handle};
            first.depth->view = {0, 1};
            auto last         = first;
            last.load = last.depth->load = LoadOp::LOAD;
            last.store = last.depth->store = StoreOp::DONT_CARE;
            data.passes.insert(data.passes.begin() + 1, std::move(last));
            return std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data));
        }
    };
} // namespace

TEST_P(MultisampleDepth, InvalidSamplesViewsAndDepthContentsFailBeforeAllocation)
{
    const auto graph = DepthGraph();
    const auto lease = Begin();
    for (uint32_t fault = 0; fault < 14; ++fault) {
        auto  data  = graph->GetData();
        auto &first = std::get<FrameRasterPass>(data.passes[0]);
        auto &last  = std::get<FrameRasterPass>(data.passes[1]);
        auto &depth = data.images[2];
        switch (fault) {
        case 0: depth.descriptor.samples = SampleCount::X1; break;
        case 1: depth.descriptor.samples = SampleCount::X2; break;
        case 2: depth.descriptor.samples = static_cast<SampleCount>(3); break;
        case 3: depth.descriptor.mipLevels = 2; break;
        case 4: first.depth->view.mip = 1; break;
        case 5: first.depth->view.layer = 2; break;
        case 6: depth.descriptor.extent.width = 8; break;
        case 7: depth.descriptor.usage = ImageUsageFlagBit::RENDER_TARGET; break;
        case 8: depth.descriptor.usage |= ImageUsageFlagBit::SAMPLED; break;
        case 9: first.depth->load = LoadOp::LOAD; break;
        case 10: first.depth->store = StoreOp::DONT_CARE; break;
        case 11: last.depth->view.layer = 0; break;
        case 12: data.imageExports.push_back(first.depth->image); break;
        case 13:
            data.images[0].descriptor.samples = SampleCount::X1;
            first.colorResolve.reset();
            last.colorResolve.reset();
            first.sideEffect = last.sideEffect = true;
            break;
        }
        const auto result = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
        EXPECT_TRUE(std::holds_alternative<FrameError>(result)) << fault;
        EXPECT_EQ(Pool().GetStats().allocations, 0u) << fault;
    }
    auto valid = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(valid));
    EXPECT_EQ(Pool().GetStats().borrowed, 2u);
    std::get<std::unique_ptr<BackendCompiledFrame>>(valid).reset();
    EXPECT_EQ(Pool().GetStats().borrowed, 0u);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(MultisampleDepth, DepthCapabilityIsQueriedIndependentlyBeforeAllocation)
{
    const auto graph = DepthGraph();
    const auto lease = Begin();
    {
        MultisampleSupportProbe probe(*device);
        probe.depthOnly = true;
        for (const bool fail : {false, true}) {
            probe.fail        = fail;
            const auto result = context->Compile(lease, graph);
            ASSERT_TRUE(std::holds_alternative<FrameError>(result));
            EXPECT_EQ(std::get<FrameError>(result).code, fail ? FrameErrorCode::COMPILE_FAILED : FrameErrorCode::UNSUPPORTED);
            if (fail)
                EXPECT_EQ(std::get<FrameError>(result).cause.code, GpuErrorCode::OUT_OF_MEMORY);
            EXPECT_EQ(Pool().GetStats().allocations, 0u);
        }
        EXPECT_EQ(probe.queries, 2u);
        auto dead = graph->GetData();
        dead.exports.clear();
        EXPECT_TRUE(
            std::holds_alternative<FrameNoWork>(context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(dead)))));
        EXPECT_EQ(probe.queries, 2u);
    }
    auto valid = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(valid));
    std::get<std::unique_ptr<BackendCompiledFrame>>(valid).reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(MultisampleDepth, ColorAndDepthBudgetCountSeparateMultisampleStorage)
{
    const auto graph    = DepthGraph();
    const auto analyzed = AnalyzeFrameGraph(*graph);
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analyzed));
    const auto        &analysis = std::get<FrameGraphAnalysis>(analyzed);
    const uint64_t     bytes    = 2 * 4 * 2 * 2 * 4 * 4;
    FrameTransientPool tooSmall(*device, {4, bytes - 1});
    EXPECT_TRUE(std::holds_alternative<FrameError>(tooSmall.Acquire(graph->GetData(), analysis, info.deviceEpoch)));
    EXPECT_EQ(tooSmall.GetStats().borrowed, 0u);
    FrameTransientPool pool(*device, {4, bytes});
    auto               first = pool.Acquire(graph->GetData(), analysis, info.deviceEpoch);
    ASSERT_TRUE(std::holds_alternative<FrameResourceBindings>(first));
    EXPECT_EQ(pool.GetStats().bytes, bytes);
    EXPECT_EQ(pool.GetStats().borrowed, 2u);
    const auto &bindings = std::get<FrameResourceBindings>(first);
    EXPECT_NE(bindings.images[0].Get(), bindings.images[2].Get());
    EXPECT_TRUE(std::holds_alternative<FrameError>(pool.Acquire(graph->GetData(), analysis, info.deviceEpoch)));
    first       = FrameError{};
    auto reused = pool.Acquire(graph->GetData(), analysis, info.deviceEpoch);
    ASSERT_TRUE(std::holds_alternative<FrameResourceBindings>(reused));
    EXPECT_EQ(pool.GetStats().allocations, 2u);
    EXPECT_EQ(pool.GetStats().reuses, 2u);
}
TEST_P(MultisampleDepth, ColorAndDepthRetireAfterCancelDrainOrGpuCompletion)
{
    for (const bool cancel : {true, false}) {
        ResetContext();
        const auto graph    = DepthGraph();
        const auto lease    = Begin();
        auto       compiled = context->Compile(lease, graph);
        auto      *plan     = std::get_if<std::unique_ptr<BackendCompiledFrame>>(&compiled);
        ASSERT_TRUE(plan);
        EXPECT_EQ(Pool().GetStats().borrowed, 2u);
        auto dispatched = context->Dispatch(*plan);
        ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
        const auto ticket = std::get<EncodingTicket>(dispatched);
        if (cancel)
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->Cancel(ticket)));
        auto       collected = context->TryCollect(ticket);
        const auto deadline  = std::chrono::steady_clock::now() + 5s;
        while (std::holds_alternative<FramePending>(collected) && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
            collected = context->TryCollect(ticket);
        }
        if (cancel) {
            EXPECT_TRUE(std::holds_alternative<EncodingCancelled>(collected));
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        } else {
            auto *recording = std::get_if<std::unique_ptr<RecordedFrame>>(&collected);
            ASSERT_TRUE(recording);
            ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(*recording)));
            device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
            EXPECT_EQ(Pool().GetStats().borrowed, 2u);
            std::array<FrameCompletion, 1> completed;
            ASSERT_EQ(context->PollCompleted(completed), 1u);
            EXPECT_EQ(completed[0].status, FrameCompletionStatus::COMPLETE);
        }
        EXPECT_EQ(Pool().GetStats().borrowed, 0u);
    }
}

INSTANTIATE_TEST_SUITE_P(Backends, MultisampleDepth, testing::Values(0u, 1u, 2u));

namespace {
    class VulkanMultisampleDepth : public MultisampleDepth {};
} // namespace
TEST_P(VulkanMultisampleDepth, DepthViewFailureRollsBackAndSuccessfulViewRetiresWithGpu)
{
    auto  graph     = DepthGraph();
    auto  data      = graph->GetData();
    auto &depth     = data.images[2];
    depth.transient = false;
    depth.image     = ImagePtr(device->CreateImage(depth.descriptor));
    ASSERT_TRUE(depth.image);
    const auto         *native = static_cast<VulkanImage *>(depth.image.Get());
    AttachmentViewProbe views(*static_cast<VulkanDevice *>(device), native->GetNativeHandle());
    graph            = std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data));
    const auto lease = Begin();
    views.failAt     = 1;
    auto failed      = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<FrameError>(failed));
    EXPECT_EQ(std::get<FrameError>(failed).code, FrameErrorCode::COMPILE_FAILED);
    EXPECT_EQ(std::get<FrameError>(failed).cause.code, GpuErrorCode::OUT_OF_MEMORY);
    EXPECT_EQ(Pool().GetStats().borrowed, 0u);
    EXPECT_TRUE(views.live.empty());
    views.failAt   = 0;
    auto  compiled = context->Compile(lease, graph);
    auto *plan     = std::get_if<std::unique_ptr<BackendCompiledFrame>>(&compiled);
    ASSERT_TRUE(plan);
    EXPECT_EQ(views.attempts, 2u);
    EXPECT_EQ(views.live.size(), 1u);
    views.frozen    = true;
    auto dispatched = context->Dispatch(*plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto       collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    const auto deadline  = std::chrono::steady_clock::now() + 5s;
    while (std::holds_alternative<FramePending>(collected) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
        collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    }
    auto *recording = std::get_if<std::unique_ptr<RecordedFrame>>(&collected);
    ASSERT_TRUE(recording);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(*recording)));
    device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
    EXPECT_EQ(views.live.size(), 1u);
    std::array<FrameCompletion, 1> completed;
    ASSERT_EQ(context->PollCompleted(completed), 1u);
    EXPECT_EQ(completed[0].status, FrameCompletionStatus::COMPLETE);
    EXPECT_TRUE(views.live.empty());
    EXPECT_EQ(views.destroyed, 1u);
    EXPECT_EQ(views.lateCreates, 0u);
    EXPECT_EQ(Pool().GetStats().borrowed, 0u);
}

INSTANTIATE_TEST_SUITE_P(VulkanModes, VulkanMultisampleDepth, testing::Values(0u, 1u));
