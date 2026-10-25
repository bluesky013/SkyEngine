//
// Created on 2026/10/16.
//

#include "GpuTestSupport.h"
#include "ShaderSourcesFixture.h"
#include "SkinningBatch.h"
#include <aurora/renderer/PassResourceBindings.h>
#include "HostSceneFixture.h"
#include "graph/FrameComposer.h"
#include <aurora/renderer/PassResourceBindings.h>

#include "graph/FrameGraphBuilder.h"
#include "graph/PreparedRaster.h"
#include "graph/RasterShaderResolver.h"
#include "prepare/PreparedScene.h"
#include "resource/ResourceUploader.h"
#include <D3D12Device.h>
#include <VulkanInstance.h>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <common/FrameTransientPool.h>
#include <d3d12sdklayers.h>

namespace {
    constexpr uint64_t epoch                           = 127;
    HRESULT (*depthClose)(ID3D12GraphicsCommandList *) = nullptr;
    uint32_t depthCloseCalls                           = 0;
    HRESULT  FailSecondDepthClose(ID3D12GraphicsCommandList *command)
    {
        return ++depthCloseCalls == 2 ? E_FAIL : depthClose(command);
    }
    struct Probe {
        std::shared_ptr<const FrozenFrameGraph> graph;
        BufferPtr                               readback;
        ResourceUploadBatch                     upload;
    };
    struct PipelineProbe {
        explicit PipelineProbe(Device &device)
        {
            active = this;
            if (device.GetAPI() == API::VULKAN) {
                vk                            = &const_cast<VulkanDeviceFunctions &>(static_cast<VulkanDevice &>(device).GetDeviceFn());
                savedVk                       = *vk;
                vk->vkCreateGraphicsPipelines = CreateVk;
                vk->vkUpdateDescriptorSets    = UpdateVk;
            } else {
                dx                         = &const_cast<D3D12Functions &>(static_cast<D3D12Device &>(device).GetFunctions());
                savedDx                    = *dx;
                dx->createGraphicsPipeline = CreateDx;
            }
        }
        ~PipelineProbe()
        {
            if (vk)
                *vk = savedVk;
            if (dx)
                *dx = savedDx;
            active = nullptr;
        }
        static VkResult VKAPI_PTR CreateVk(VkDevice                            device,
                                           VkPipelineCache                     cache,
                                           uint32_t                            count,
                                           const VkGraphicsPipelineCreateInfo *info,
                                           const VkAllocationCallbacks        *allocator,
                                           VkPipeline                         *out)
        {
            active->pipelines += count;
            if (active->frozen)
                ++active->lateWrites;
            return active->savedVk.vkCreateGraphicsPipelines(device, cache, count, info, allocator, out);
        }
        static void VKAPI_PTR
        UpdateVk(VkDevice device, uint32_t count, const VkWriteDescriptorSet *writes, uint32_t copies, const VkCopyDescriptorSet *copy)
        {
            active->descriptors += count;
            if (active->frozen)
                ++active->lateWrites;
            active->savedVk.vkUpdateDescriptorSets(device, count, writes, copies, copy);
        }
        static HRESULT CreateDx(ID3D12Device *device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc, ID3D12PipelineState **out)
        {
            ++active->pipelines;
            if (active->frozen)
                ++active->lateWrites;
            return active->savedDx.createGraphicsPipeline(device, desc, out);
        }
        inline static PipelineProbe *active = nullptr;
        std::atomic_uint             pipelines{0}, descriptors{0}, lateWrites{0};
        std::atomic_bool             frozen{false};
        VulkanDeviceFunctions       *vk = nullptr;
        VulkanDeviceFunctions        savedVk{};
        D3D12Functions              *dx = nullptr;
        D3D12Functions               savedDx;
    };
    class IndexedRaster : public testing::TestWithParam<uint32_t> {
    protected:
        void SetUp() override
        {
            Instance::Get()->Init({"IndexedRasterTest", "SkyEngine", true, GetParam() == 2 ? API::DX12 : API::VULKAN});
            device = Instance::Get()->GetDevice();
            ASSERT_TRUE(device);
            if (GetParam() == 2) {
                ASSERT_TRUE(SUCCEEDED(static_cast<D3D12Device *>(device)->GetNativeHandle()->QueryInterface(IID_PPV_ARGS(&debug))));
                debug->ClearStoredMessages();
            }
            info.deviceEpoch    = epoch;
            info.parallelNum    = 3;
            info.vulkanCompiler = GetParam() == 1 ? VulkanFrameGraphMode::RENDER_PASS : VulkanFrameGraphMode::DYNAMIC_RENDERING;
            ResetContext();
            shaders[0] = ShaderOf(false);
            shaders[1] = ShaderOf(true);
            ASSERT_TRUE(shaders[0] && shaders[1]);
        }
        void TearDown() override
        {
            uploader.reset();
            catalog.reset();
            context.reset();
            shaders = {};
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
        ShaderPtr ShaderOf(bool alternate)
        {
            const auto source = std::string(alternate ? "#define MATERIAL_BINDING 1\n" : "#define MATERIAL_BINDING 0\n") + kGpuIndexedSource;
            const auto vs     = CompileGpuFunction(*device, source, ShaderStageFlagBit::VS, "VSMain");
            const auto fs     = CompileGpuFunction(*device, source, ShaderStageFlagBit::FS, "FSMain");
            if (!vs || !fs)
                return {};
            Shader::Descriptor descriptor{};
            descriptor.vs              = vs.Get();
            descriptor.ps              = fs.Get();
            descriptor.bindingPolicies = {{0, 0}, {2, 0}};
            return ShaderPtr(device->CreateShader(descriptor));
        }
        DeviceFrameLease Begin()
        {
            auto result = context->TryBeginSlot();
            EXPECT_TRUE(std::holds_alternative<DeviceFrameLease>(result));
            auto *lease = std::get_if<DeviceFrameLease>(&result);
            return lease ? *lease : DeviceFrameLease{};
        }
        GraphBufferHandle Import(FrameGraphBuilder &builder, BufferPtr buffer, CopyBufferState state, bool initialized)
        {
            auto result = builder.Import(buffer, state, state, initialized);
            EXPECT_TRUE(std::holds_alternative<GraphBufferHandle>(result));
            auto *handle = std::get_if<GraphBufferHandle>(&result);
            return handle ? *handle : GraphBufferHandle{};
        }
        FrameBufferSlice Uniform(const DeviceFrameLease &lease, std::array<float, 4> values)
        {
            auto result = context->WriteFrameBuffer(lease, FrameBufferKind::UNIFORM, std::as_bytes(std::span{values}));
            EXPECT_TRUE(std::holds_alternative<FrameBufferSlice>(result));
            auto *slice = std::get_if<FrameBufferSlice>(&result);
            return slice ? *slice : FrameBufferSlice{};
        }
        GraphImageHandle ImageOf(FrameGraphBuilder &builder, bool depth)
        {
            FrameGraphImage image;
            image.descriptor.format = depth ? PixelFormat::D32 : PixelFormat::RGBA8_UNORM;
            image.descriptor.extent = {8, 8, 1};
            image.descriptor.usage =
                depth ? ImageUsageFlags(ImageUsageFlagBit::DEPTH_STENCIL) : ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
            image.final = depth ? ImageLayout::DEPTH_STENCIL_ATTACHMENT : ImageLayout::TRANSFER_SRC;
            image.image = ImagePtr(device->CreateImage(image.descriptor));
            EXPECT_TRUE(image.image);
            auto result = builder.Import(std::move(image));
            EXPECT_TRUE(std::holds_alternative<GraphImageHandle>(result));
            auto *handle = std::get_if<GraphImageHandle>(&result);
            return handle ? *handle : GraphImageHandle{};
        }
        Probe MakeGraph(const DeviceFrameLease &lease, IndexType type = IndexType::U16, uint32_t count = 70, bool upload = false, bool split = false)
        {
            Probe                result;
            FrameGraphBuilder    builder;
            std::vector<uint8_t> geometry(128);
            const float          positions[] = {-1, -1, -1, 3, 3, -1};
            const float          colors[]    = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
            const uint16_t       indices16[] = {99, 0, 1, 2};
            const uint32_t       indices32[] = {99, 0, 1, 2};
            std::memcpy(geometry.data(), positions, sizeof(positions));
            std::memcpy(geometry.data() + 32, colors, sizeof(colors));
            std::memcpy(geometry.data() + 80, indices16, sizeof(indices16));
            std::memcpy(geometry.data() + 96, indices32, sizeof(indices32));
            GraphBufferHandle mesh;
            if (upload) {
                const SceneId           scene{0, 1};
                const RenderEntityRef   entity{scene, {0, 1}};
                const RenderResourceKey key{Uuid::CreateWithSeed(127), 1, 1, epoch};
                RenderEntityDesc        desc;
                desc.mesh = {key, 1};
                CpuSceneStore store;
                FramePacket   packet;
                packet.sequence = 1;
                packet.commands = {CreateScene{scene, {}}, CreateRenderEntity{entity, desc}};
                EXPECT_TRUE(store.Apply(packet).Succeeded());
                catalog  = std::move(std::get<std::unique_ptr<ResourceCatalog>>(ResourceCatalog::Create({epoch})));
                uploader = std::move(std::get<std::unique_ptr<ResourceUploader>>(ResourceUploader::Create(*device, *context, *catalog)));
                const auto            requested = catalog->Request(store, {entity, ResourceBindingSlot::MESH, 1, 1}, false);
                const auto            ticket    = std::get<ResourceRequest>(requested).ticket;
                RenderResourcePayload payload{key, Name("indexed-mesh-v1"), std::move(geometry), {}};
                EXPECT_FALSE(catalog->Supply(ticket, payload));
                auto prepared = uploader->Prepare(lease, builder, std::span{&ticket, 1});
                EXPECT_TRUE(std::holds_alternative<ResourceUploadBatch>(prepared));
                result.upload = std::move(std::get<ResourceUploadBatch>(prepared));
                mesh          = std::get<GraphBufferHandle>(uploader->ResolveInGraph(result.upload.id, ticket, builder));
            } else {
                auto               written = context->WriteFrameBuffer(lease, FrameBufferKind::STAGING, std::as_bytes(std::span{geometry}), 4);
                auto              &slice   = std::get<FrameBufferSlice>(written);
                auto               source  = Import(builder, slice.buffer, CopyBufferState::HOST_WRITE, true);
                Buffer::Descriptor desc;
                desc.size  = geometry.size();
                desc.usage = BufferUsageFlagBit::TRANSFER_DST | BufferUsageFlagBit::VERTEX | BufferUsageFlagBit::INDEX;
                mesh       = Import(builder, BufferPtr(device->CreateBuffer(desc)), CopyBufferState::COMMON, false);
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, mesh, slice.offset, 0, desc.size})));
            }
            const auto      left     = Uniform(lease, {0, 0, .2f, 0});
            const auto      right    = Uniform(lease, {0, 0, .6f, 0});
            const auto      middle   = Uniform(lease, {0, 0, .4f, 0});
            const auto      distant  = Uniform(lease, {0, 0, .9f, 0});
            const auto      red      = Uniform(lease, {1, 0, 0, 1});
            const auto      green    = Uniform(lease, {0, 1, 0, 1});
            const auto      blue     = Uniform(lease, {0, 0, 1, 1});
            const auto      scale    = Uniform(lease, {1, 1, 0, 0});
            const auto      uniforms = Import(builder, left.buffer, CopyBufferState::HOST_WRITE, true);
            FrameRasterPass pass;
            pass.color       = ImageOf(builder, false);
            pass.depth       = RasterDepthAttachment{ImageOf(builder, true)};
            const auto color = pass.color;
            for (uint32_t i = 0; i < count; ++i) {
                const bool  alternate = i % 2;
                const auto &view      = i == 0 ? left : i == 1 ? right : i == 2 ? middle : distant;
                const auto &tint      = i == 0 ? red : i == 1 ? green : blue;
                RasterDraw  draw;
                draw.shader     = shaders[alternate];
                draw.vertices   = {{{mesh, 0, 24}, 8}, {{mesh, 32, 48}, 16}};
                draw.attributes = {{0, 0, 0, Format::F_RG32, "POSITION", 0}, {1, 1, 0, Format::F_RGBA32, "COLOR", 0}};
                draw.indexed =
                    RasterIndexedDraw{{mesh, type == IndexType::U16 ? 80u : 96u, type == IndexType::U16 ? 8u : 16u}, type, {3, 1, 1, 0, 0}};
                draw.uniforms = {{2, 0, {uniforms, 0, scale.size}, static_cast<uint32_t>(scale.offset)},
                                 {0, 0, {uniforms, 0, view.size}, static_cast<uint32_t>(view.offset)},
                                 {1, alternate ? 1u : 0u, {uniforms, tint.offset, tint.size}, 0}};
                draw.depth    = {true, true, CompareOp::LESS};
                if (i < 2)
                    draw.scissor = {{static_cast<int32_t>(i * 4), 0}, {4, 8}};
                if (split && i == 2) {
                    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
                    pass.draws.clear();
                    pass.load        = LoadOp::LOAD;
                    pass.depth->load = LoadOp::LOAD;
                }
                pass.draws.push_back(std::move(draw));
            }
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(pass))));
            Buffer::Descriptor readback;
            readback.size          = 8 * 8 * 4;
            readback.usage         = BufferUsageFlagBit::TRANSFER_DST;
            readback.memory        = GetParam() == 2 ? MemoryType::GPU_TO_CPU : MemoryType::CPU_ONLY;
            result.readback        = BufferPtr(device->CreateBuffer(readback));
            const auto      output = Import(builder, result.readback, CopyBufferState::HOST_READ, false);
            BufferImageCopy region;
            region.imageExtent = {8, 8, 1};
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, color, region, ImageCopyDirection::IMAGE_TO_BUFFER})));
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({output, 0, readback.size})));
            result.graph = std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
            return result;
        }
        std::unique_ptr<RecordedFrame> Record(const DeviceFrameLease &lease, const Probe &probe)
        {
            auto  compiled = context->Compile(lease, probe.graph);
            auto *plan     = std::get_if<std::unique_ptr<BackendCompiledFrame>>(&compiled);
            if (!plan) {
                if (auto *error = std::get_if<FrameError>(&compiled))
                    ADD_FAILURE() << static_cast<int>(error->code) << " " << error->message << " " << error->cause.nativeCode;
                return {};
            }
            EXPECT_TRUE(
                std::holds_alternative<FrameError>(context->WriteFrameBuffer(lease, FrameBufferKind::UNIFORM, std::as_bytes(std::span("frozen")))));
            if (afterCompile)
                afterCompile();
            auto dispatch = context->Dispatch(*plan);
            EXPECT_TRUE(std::holds_alternative<EncodingTicket>(dispatch));
            if (!std::holds_alternative<EncodingTicket>(dispatch))
                return {};
            const auto ticket    = std::get<EncodingTicket>(dispatch);
            auto       collected = context->TryCollect(ticket);
            const auto deadline  = std::chrono::steady_clock::now() + 5s;
            while (std::holds_alternative<FramePending>(collected) && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(1ms);
                collected = context->TryCollect(ticket);
            }
            EXPECT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
            auto *recorded = std::get_if<std::unique_ptr<RecordedFrame>>(&collected);
            return recorded ? std::move(*recorded) : nullptr;
        }
        void Complete(uint32_t expected)
        {
            device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
            std::array<FrameCompletion, 4> completed;
            const auto                     count = uploader ? uploader->PollCompleted(completed) : context->PollCompleted(completed);
            ASSERT_EQ(count, expected);
            for (uint32_t i = 0; i < count; ++i)
                EXPECT_EQ(completed[i].status, FrameCompletionStatus::COMPLETE);
        }
        void Pixels(const Probe &probe, bool blueRight = true)
        {
            auto *bytes = probe.readback->Map();
            ASSERT_TRUE(bytes);
            for (uint32_t y = 0; y < 8; ++y)
                for (uint32_t x = 0; x < 8; ++x) {
                    const uint32_t offset = (y * 8 + x) * 4;
                    EXPECT_EQ(bytes[offset], x < 4 ? 255 : 0) << x << "," << y;
                    EXPECT_EQ(bytes[offset + 1], x >= 4 && !blueRight ? 255 : 0);
                    EXPECT_EQ(bytes[offset + 2], x >= 4 && blueRight ? 255 : 0);
                    EXPECT_EQ(bytes[offset + 3], 255);
                }
            probe.readback->UnMap();
        }
        Device                             *device = nullptr;
        DeviceFrameContextInitInfo          info;
        std::unique_ptr<DeviceFrameContext> context;
        std::array<ShaderPtr, 2>            shaders;
        ComPtr<ID3D12InfoQueue>             debug;
        std::unique_ptr<ResourceCatalog>    catalog;
        std::unique_ptr<ResourceUploader>   uploader;
        std::function<void()>               afterCompile;
    };
} // namespace

TEST_P(IndexedRaster, MultiStreamIndexTypesAndViewsMatchSerialAndParallel)
{
    for (auto workers : {1u, 3u}) {
        info.parallelNum = workers;
        ResetContext();
        std::array<Probe, 2> probes;
        for (uint32_t i = 0; i < probes.size(); ++i) {
            const auto lease = Begin();
            probes[i]        = MakeGraph(lease, i ? IndexType::U32 : IndexType::U16);
            auto analyzed    = AnalyzeFrameGraph(*probes[i].graph);
            ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analyzed));
            EXPECT_EQ(std::get<FrameGraphAnalysis>(analyzed).livePasses.size(), 3u);
            auto recording = Record(lease, probes[i]);
            ASSERT_TRUE(recording);
            ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
        }
        Complete(2);
        for (const auto &probe : probes)
            Pixels(probe);
    }
}

TEST_P(IndexedRaster, PipelineCacheReusesAcrossFramesAndSeparatesStateAndEpoch)
{
    PipelineProbe native(*device);
    for (uint32_t frame = 0; frame < 4; ++frame) {
        if (frame == 3) {
            ++info.deviceEpoch;
            ResetContext();
        }
        native.frozen    = false;
        const auto lease = Begin();
        auto       probe = MakeGraph(lease);
        if (frame == 2) {
            auto data                                                        = probe.graph->GetData();
            std::get<FrameRasterPass>(data.passes[1]).draws[0].depth.compare = CompareOp::LESS_OR_EQUAL;
            probe.graph = std::make_shared<const FrozenFrameGraph>(probe.graph->GetId(), std::move(data));
        }
        afterCompile   = [&] { native.frozen = true; };
        auto recording = Record(lease, probe);
        ASSERT_TRUE(recording);
        EXPECT_EQ(native.pipelines.load(), frame < 2 ? 2u : frame == 2 ? 3u : 5u);
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
        Complete(1);
        Pixels(probe);
        EXPECT_EQ(native.lateWrites.load(), 0u);
    }
}

TEST_P(IndexedRaster, DepthLoadAcrossScopesPreservesOcclusion)
{
    const auto lease     = Begin();
    auto       probe     = MakeGraph(lease, IndexType::U16, 70, false, true);
    auto       recording = Record(lease, probe);
    ASSERT_TRUE(recording);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete(1);
    Pixels(probe);
}

TEST_P(IndexedRaster, DepthResolveMinMaxPreserveSelectedCellAndDiscardedSource)
{
    info.inflightNum = 1;
    std::array<std::vector<uint8_t>, 2> serialPixels;
    uint32_t                            supported = 0;
    for (auto samples : {SampleCount::X2, SampleCount::X4, SampleCount::X8}) {
        for (auto workers : {1u, 3u}) {
            info.parallelNum = workers;
            ResetContext();
            std::array<uint32_t, 2> passed{};
            for (uint32_t mode = 0; mode < 2; ++mode) {
                SCOPED_TRACE(::testing::Message() << "samples=" << static_cast<uint32_t>(samples) << " workers=" << workers << " mode=" << mode);
                auto       lease    = Begin();
                auto       probe    = MakeGraph(lease, IndexType::U16, 3);
                const auto partial  = Uniform(lease, {.4f, 4.f, 0, 0});
                auto       data     = probe.graph->GetData();
                auto       original = std::get<FrameRasterPass>(data.passes[1]);
                auto       addImage = [&](bool depth, SampleCount count, bool target) {
                    FrameGraphImage image;
                    image.transient              = true;
                    image.descriptor.format      = depth ? PixelFormat::D32 : PixelFormat::RGBA8_UNORM;
                    image.descriptor.extent      = target ? Extent3D{16, 16, 1} : Extent3D{8, 8, 1};
                    image.descriptor.mipLevels   = target ? 2 : 1;
                    image.descriptor.arrayLayers = workers;
                    image.descriptor.samples     = count;
                    image.descriptor.usage =
                        depth ? ImageUsageFlags(ImageUsageFlagBit::DEPTH_STENCIL) : ImageUsageFlags(ImageUsageFlagBit::RENDER_TARGET);
                    image.final = ImageLayout::GENERAL;
                    GraphImageHandle handle{probe.graph->GetId(), static_cast<uint32_t>(data.images.size())};
                    data.images.push_back(std::move(image));
                    return handle;
                };
                FrameRasterPass source = original;
                source.color           = addImage(false, samples, false);
                source.colorView       = {0, workers - 1};
                source.depth           = RasterDepthAttachment{addImage(true, samples, false)};
                source.depth->view     = {0, workers - 1};
                source.depth->store    = mode ? StoreOp::STORE : StoreOp::DONT_CARE;
                const auto resolved    = addImage(true, SampleCount::X1, true);
                source.colorResolve    = RasterColorResolve{addImage(false, SampleCount::X1, true), {1, workers - 1}};
                source.depth->resolve  = RasterDepthResolve{resolved, {1, workers - 1}, mode ? DepthResolveMode::MAXIMUM : DepthResolveMode::MINIMUM};
                source.draws.resize(1);
                source.draws[0].scissor                   = {};
                source.draws[0].uniforms[0].dynamicOffset = static_cast<uint32_t>(partial.offset);
                source.draws.resize(70, source.draws[0]);
                FrameRasterPass poison = original;
                poison.draws.clear();
                poison.depth            = RasterDepthAttachment{resolved};
                poison.depth->view      = {1, 0};
                poison.depth->clear     = 0;
                poison.sideEffect       = true;
                FrameRasterPass consume = original;
                consume.depth           = RasterDepthAttachment{resolved, LoadOp::LOAD};
                consume.depth->view     = {1, workers - 1};
                consume.draws           = {original.draws[2]};
                auto clear              = source;
                if (workers == 3) {
                    clear.draws.clear();
                    clear.colorResolve.reset();
                    clear.depth->resolve.reset();
                    clear.depth->store = StoreOp::STORE;
                    source.load        = LoadOp::LOAD;
                    source.depth->load = LoadOp::LOAD;
                }
                data.passes[1] = source;
                data.passes.insert(data.passes.begin() + 2, consume);
                if (workers == 3) {
                    data.passes.insert(data.passes.begin() + 2, poison);
                    data.passes.insert(data.passes.begin() + 1, std::move(clear));
                }
                probe.graph = std::make_shared<const FrozenFrameGraph>(probe.graph->GetId(), std::move(data));
                if (samples == SampleCount::X2 && workers == 1 && mode == 0) {
                    for (uint32_t fault = 0; fault < 7; ++fault) {
                        auto  invalid = probe.graph->GetData();
                        auto &depth   = *std::get<FrameRasterPass>(invalid.passes[1]).depth;
                        auto &target  = invalid.images[depth.resolve->image.index].descriptor;
                        switch (fault) {
                        case 0: depth.resolve->mode = DepthResolveMode::UNDEFINED; break;
                        case 1: depth.resolve->image = depth.image; break;
                        case 2: depth.load = LoadOp::DONT_CARE; break;
                        case 3: target.format = PixelFormat::RGBA8_UNORM; break;
                        case 4: target.extent.width = 32; break;
                        case 5: depth.resolve->view.mip = 2; break;
                        case 6: depth.resolve->image.graph++; break;
                        }
                        EXPECT_TRUE(
                            std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(probe.graph->GetId(), std::move(invalid)))));
                    }
                    if (GetParam() == 2) {
                        auto      &native = *static_cast<D3D12Device *>(device);
                        auto      &fn     = const_cast<D3D12Functions &>(native.GetFunctions());
                        const auto saved  = fn;
                        const auto before = native.GetTransientPool().GetStats().allocations;
                        fn.options2       = [](ID3D12Device *, D3D12_FEATURE_DATA_D3D12_OPTIONS2 *out) -> HRESULT {
                            *out = {};
                            return S_OK;
                        };
                        auto unsupported = context->Compile(lease, probe.graph);
                        fn               = saved;
                        ASSERT_TRUE(std::holds_alternative<FrameError>(unsupported));
                        EXPECT_EQ(std::get<FrameError>(unsupported).code, FrameErrorCode::UNSUPPORTED);
                        fn.options2 = [](ID3D12Device *, D3D12_FEATURE_DATA_D3D12_OPTIONS2 *) -> HRESULT { return E_OUTOFMEMORY; };
                        auto failed = context->Compile(lease, probe.graph);
                        fn          = saved;
                        ASSERT_TRUE(std::holds_alternative<FrameError>(failed));
                        EXPECT_EQ(std::get<FrameError>(failed).cause.nativeCode, static_cast<int64_t>(E_OUTOFMEMORY));
                        fn.options2 = [](ID3D12Device *, D3D12_FEATURE_DATA_D3D12_OPTIONS2 *out) -> HRESULT {
                            *out                                 = {};
                            out->ProgrammableSamplePositionsTier = D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_1;
                            return S_OK;
                        };
                        fn.createList1 = [](ID3D12Device *, D3D12_COMMAND_LIST_TYPE, ID3D12CommandAllocator *,
                                            ID3D12GraphicsCommandList1 **) -> HRESULT { return E_NOINTERFACE; };
                        unsupported    = context->Compile(lease, probe.graph);
                        fn             = saved;
                        ASSERT_TRUE(std::holds_alternative<FrameError>(unsupported));
                        EXPECT_EQ(std::get<FrameError>(unsupported).code, FrameErrorCode::UNSUPPORTED);
                        EXPECT_EQ(native.GetTransientPool().GetStats().allocations, before);
                    } else {
                        auto      &native                 = *static_cast<VulkanDevice *>(device);
                        auto      &fn                     = const_cast<VulkanInstanceFunctions &>(native.GetVulkanInstance().GetInstanceFn());
                        const auto saved                  = fn.vkGetPhysicalDeviceProperties2;
                        const auto before                 = native.GetTransientPool().GetStats().allocations;
                        fn.vkGetPhysicalDeviceProperties2 = [](VkPhysicalDevice, VkPhysicalDeviceProperties2 *out) {
                            static_cast<VkPhysicalDeviceDepthStencilResolveProperties *>(out->pNext)->supportedDepthResolveModes =
                                VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
                        };
                        auto unsupported                  = context->Compile(lease, probe.graph);
                        fn.vkGetPhysicalDeviceProperties2 = saved;
                        ASSERT_TRUE(std::holds_alternative<FrameError>(unsupported));
                        EXPECT_EQ(std::get<FrameError>(unsupported).code, FrameErrorCode::UNSUPPORTED);
                        EXPECT_EQ(native.GetTransientPool().GetStats().allocations, before);
                    }
                }
                auto compiled = context->Compile(lease, probe.graph);
                if (const auto *error = std::get_if<FrameError>(&compiled); error && error->code == FrameErrorCode::UNSUPPORTED) {
                    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
                    continue;
                }
                ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
                ++supported;
                auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
                if (GetParam() == 2 && samples == SampleCount::X2 && workers == 1 && mode == 0) {
                    auto &fn            = const_cast<D3D12Functions &>(static_cast<D3D12Device *>(device)->GetFunctions());
                    depthClose          = fn.closeList;
                    depthCloseCalls     = 0;
                    fn.closeList        = FailSecondDepthClose;
                    auto failedDispatch = context->Dispatch(plan);
                    fn.closeList        = depthClose;
                    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(failedDispatch));
                    auto failed = context->TryCollect(std::get<EncodingTicket>(failedDispatch));
                    ASSERT_TRUE(std::holds_alternative<EncodingFailed>(failed));
                    EXPECT_EQ(std::get<EncodingFailed>(failed).error.cause.nativeCode, static_cast<int64_t>(E_FAIL));
                    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
                    lease    = Begin();
                    compiled = context->Compile(lease, probe.graph);
                    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
                    plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
                }
                auto dispatched = context->Dispatch(plan);
                ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
                auto       ticket    = std::get<EncodingTicket>(dispatched);
                auto       collected = context->TryCollect(ticket);
                const auto deadline  = std::chrono::steady_clock::now() + 5s;
                while (std::holds_alternative<FramePending>(collected) && std::chrono::steady_clock::now() < deadline) {
                    std::this_thread::sleep_for(1ms);
                    collected = context->TryCollect(ticket);
                }
                ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
                auto recording = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
                ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
                Complete(1);
                auto *bytes = probe.readback->Map();
                ASSERT_TRUE(bytes);
                std::vector<uint8_t> pixels(bytes, bytes + 256);
                probe.readback->UnMap();
                for (uint32_t i = 0; i < 64; ++i) {
                    EXPECT_EQ(pixels[i * 4], 0);
                    EXPECT_EQ(pixels[i * 4 + 1], 0);
                    passed[mode] += pixels[i * 4 + 2] == 255;
                }
                EXPECT_GT(passed[mode], 0u);
                EXPECT_LT(passed[mode], 64u);
                if (workers == 1)
                    serialPixels[mode] = pixels;
                else
                    EXPECT_EQ(serialPixels[mode], pixels);
            }
            if (passed[0] && passed[1])
                EXPECT_LT(passed[0], passed[1]);
        }
    }
    if (!supported)
        GTEST_SKIP() << "Native MIN/MAX depth resolve is unavailable";
}

TEST_P(IndexedRaster, DepthClippingMatchesWithDepthTestDisabled)
{
    const auto lease               = Begin();
    auto       probe               = MakeGraph(lease, IndexType::U32, 3);
    const auto outside             = Uniform(lease, {0, 0, 2, 0});
    auto       data                = probe.graph->GetData();
    auto      &draw                = std::get<FrameRasterPass>(data.passes[1]).draws.back();
    draw.depth                     = {};
    draw.uniforms[1].dynamicOffset = static_cast<uint32_t>(outside.offset);
    probe.graph                    = std::make_shared<const FrozenFrameGraph>(probe.graph->GetId(), std::move(data));
    auto recording                 = Record(lease, probe);
    ASSERT_TRUE(recording);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete(1);
    Pixels(probe, false);
}

TEST_P(IndexedRaster, FirstResourceUploadFeedsIndexedDrawBeforeResident)
{
    const auto lease     = Begin();
    auto       probe     = MakeGraph(lease, IndexType::U16, 3, true);
    auto       recording = Record(lease, probe);
    ASSERT_TRUE(recording);
    const auto ticket = probe.upload.admitted.front();
    EXPECT_EQ(std::get<ResourceCatalogStatus>(catalog->GetStatus(ticket)).state, RenderResourceState::UPLOAD_QUEUED);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(probe.upload.id, recording)));
    EXPECT_EQ(std::get<ResourceCatalogStatus>(catalog->GetStatus(ticket)).state, RenderResourceState::UPLOAD_SUBMITTED);
    Complete(1);
    EXPECT_EQ(std::get<ResourceCatalogStatus>(catalog->GetStatus(ticket)).state, RenderResourceState::RESIDENT);
    Pixels(probe);
}

TEST_P(IndexedRaster, InvalidRangesBindingsAndDepthAreRejected)
{
    const auto lease = Begin();
    auto       probe = MakeGraph(lease, IndexType::U16, 3);
    for (uint32_t fault = 0; fault < 12; ++fault) {
        auto  data = probe.graph->GetData();
        auto &pass = std::get<FrameRasterPass>(data.passes[1]);
        auto &draw = pass.draws.front();
        switch (fault) {
        case 0: draw.indexed->range.size = 2; break;
        case 1: draw.vertices[0].range.size = UINT64_MAX; break;
        case 2: draw.attributes[0].binding = 9; break;
        case 3: draw.uniforms[0].range.buffer.graph++; break;
        case 4: draw.uniforms[0].dynamicOffset = UINT32_MAX; break;
        case 5: pass.depth->load = LoadOp::DONT_CARE; break;
        case 6: pass.depth.reset(); break;
        case 7: draw.uniforms.pop_back(); break;
        case 8: draw.uniforms[0].binding = 7; break;
        case 9: draw.uniforms[0].dynamicOffset = 1; break;
        case 10: draw.uniforms[2].dynamicOffset = 256; break;
        case 11: data.passes.erase(data.passes.begin()); break;
        }
        auto graph = std::make_shared<const FrozenFrameGraph>(probe.graph->GetId(), std::move(data));
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->Compile(lease, graph))) << fault;
    }
    auto recording = Record(lease, probe);
    ASSERT_TRUE(recording);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete(1);
    Pixels(probe);
}

TEST_P(IndexedRaster, DescriptorBudgetFailureRollsBackForSameLeaseRetry)
{
    info.allocations.resourceDescriptors = 3;
    info.allocations.descriptorSets      = 3;
    ResetContext();
    const auto lease = Begin();
    auto       probe = MakeGraph(lease, IndexType::U16, 4);
    for (uint32_t retry = 0; retry < 2; ++retry) {
        auto result = context->Compile(lease, probe.graph);
        ASSERT_TRUE(std::holds_alternative<FrameError>(result));
        EXPECT_EQ(std::get<FrameError>(result).code, FrameErrorCode::OUT_OF_MEMORY);
    }
    auto  data = probe.graph->GetData();
    auto &pass = std::get<FrameRasterPass>(data.passes[1]);
    pass.draws.resize(1);
    probe.graph    = std::make_shared<const FrozenFrameGraph>(probe.graph->GetId(), std::move(data));
    auto recording = Record(lease, probe);
    ASSERT_TRUE(recording);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete(1);
}

TEST_P(IndexedRaster, SlangResolverBuildsProgramBeforeGraphFreeze)
{
    ShaderCompilerSlang compiler;
    auto               *previous = ShaderCompilerFactory::Get().GetCompiler();
    ShaderCompilerFactory::Get().Register(&compiler);
    Finally          restore{[&] { ShaderCompilerFactory::Get().Register(previous); }};
    ShaderFileSystem source;
    source.AddVirtualFile("Indexed.slang", std::string(kGpuResourceAttributes) + "#define MATERIAL_BINDING 0\n" + kGpuIndexedSource);
    ShaderResolver      resolver(nullptr, nullptr, source, 1, 1);
    ShaderVariantSchema schema;
    RasterShaderRequest request;
    request.vertex.relativePath = "Indexed.slang";
    request.vertex.entry        = "VSMain";
    request.vertex.stage        = ShaderStageFlagBit::VS;
    request.vertex.target       = static_cast<uint32_t>(GetParam() == 2 ? ShaderTarget::DXIL : ShaderTarget::SPIRV);
    request.vertex.schema       = &schema;
    request.vertex.schemaFp     = HashSchema(schema);
    request.fragment            = request.vertex;
    request.fragment.entry      = "FSMain";
    request.fragment.stage      = ShaderStageFlagBit::FS;
    request.dynamicUniforms     = {{0, 0}, {2, 0}};
    auto resolved               = ResolveRasterShader(*device, resolver, request);
    if (auto *error = std::get_if<FrameError>(&resolved))
        FAIL() << error->message;
    shaders[0]           = std::get<ShaderPtr>(resolved);
    const auto lease     = Begin();
    auto       probe     = MakeGraph(lease, IndexType::U32, 3);
    auto       recording = Record(lease, probe);
    ASSERT_TRUE(recording);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete(1);
    Pixels(probe);
    request.dynamicUniforms.push_back({0, 0});
    EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveRasterShader(*device, resolver, request)));
    request.vertex.stage = ShaderStageFlagBit::FS;
    EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveRasterShader(*device, resolver, request)));
}

TEST_P(IndexedRaster, PipelinesAreSharedAndWorkersDoNotResolveBindings)
{
    PipelineProbe native(*device);
    const auto    lease = Begin();
    auto          probe = MakeGraph(lease);
    afterCompile        = [&] {
        EXPECT_EQ(native.pipelines.load(), 2u);
        if (GetParam() != 2)
            EXPECT_EQ(native.descriptors.load(), 15u); // Five distinct binding combinations, each with three sets.
        native.frozen = true;
    };
    auto recording = Record(lease, probe);
    afterCompile   = {};
    ASSERT_TRUE(recording);
    EXPECT_EQ(native.lateWrites.load(), 0u);
    EXPECT_EQ(native.pipelines.load(), 2u);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete(1);
    Pixels(probe);
}

TEST_P(IndexedRaster, SparseShaderSetsKeepTheirActualSetNumbers)
{
    ShaderCompilerSlang compiler;
    auto               *previous = ShaderCompilerFactory::Get().GetCompiler();
    ShaderCompilerFactory::Get().Register(&compiler);
    Finally          restore{[&] { ShaderCompilerFactory::Get().Register(previous); }};
    ShaderFileSystem source;
    source.AddVirtualFile("Sparse.slang", std::string(kGpuResourceAttributes) + R"(
        struct Params { [AuroraField("tint")] float4 tint; };
        [AuroraResource(2,0,"fixture/sparse")] [[vk::binding(0,2)]] ConstantBuffer<Params> material : register(b0, space2);
        struct Varying { float4 position : SV_Position; float4 color : COLOR; };
        Varying VSMain(float2 position : POSITION, float4 color : COLOR) {
            Varying v; v.position = float4(position, 0, 1); v.color = color; return v;
        }
        float4 FSMain(Varying v) : SV_Target { return v.color * material.tint; }
    )");
    ShaderResolver      resolver(nullptr, nullptr, source, 1, 1);
    ShaderVariantSchema schema;
    RasterShaderRequest request;
    request.vertex.relativePath = "Sparse.slang";
    request.vertex.entry        = "VSMain";
    request.vertex.stage        = ShaderStageFlagBit::VS;
    request.vertex.target       = static_cast<uint32_t>(GetParam() == 2 ? ShaderTarget::DXIL : ShaderTarget::SPIRV);
    request.vertex.schema       = &schema;
    request.vertex.schemaFp     = HashSchema(schema);
    request.fragment            = request.vertex;
    request.fragment.entry      = "FSMain";
    request.fragment.stage      = ShaderStageFlagBit::FS;
    request.dynamicUniforms     = {{2, 0}};
    auto resolved               = ResolveRasterShader(*device, resolver, request);
    if (auto *error = std::get_if<FrameError>(&resolved))
        FAIL() << error->message;
    const auto lease = Begin();
    auto       probe = MakeGraph(lease, IndexType::U16, 2);
    auto       data  = probe.graph->GetData();
    for (auto &draw : std::get<FrameRasterPass>(data.passes[1]).draws) {
        draw.shader     = std::get<ShaderPtr>(resolved);
        const auto tint = draw.uniforms.back();
        draw.uniforms   = {{2, 0, {tint.range.buffer, 0, tint.range.size}, static_cast<uint32_t>(tint.range.offset)}};
        draw.depth      = {};
    }
    probe.graph    = std::make_shared<const FrozenFrameGraph>(probe.graph->GetId(), std::move(data));
    auto recording = Record(lease, probe);
    ASSERT_TRUE(recording);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete(1);
    Pixels(probe, false);
}

TEST_P(IndexedRaster, PreparedSceneViewsResolveExactFallbackAndDrawOwnedPackets)
{
    const auto materialSource = HostMaterial({device->GetAPI(), epoch});
    auto       resolved       = CreateFrameShader(*device, materialSource.material->techniques.front().program);
    ASSERT_TRUE(std::holds_alternative<ShaderPtr>(resolved));
    const auto              shader = std::get<ShaderPtr>(resolved);
    const SceneId           scene{0, 1};
    const ViewportId        viewport{0, 1};
    const RenderResourceKey mesh{Uuid::CreateWithSeed(751), 1, 1, epoch};
    const RenderResourceKey red{Uuid::CreateWithSeed(752), 1, 1, epoch}, blue{Uuid::CreateWithSeed(753), 1, 1, epoch};
    CpuSceneStore           store;
    FramePacket             packet;
    packet.sequence = 1;
    packet.commands = {CreateScene{scene, {}}, CreateViewport{viewport, {}}};
    for (uint32_t i = 0; i != 2; ++i) {
        RenderEntityDesc entity;
        entity.mesh       = {mesh, 1};
        entity.material   = {i ? blue : red, 1};
        entity.world.v[0] = entity.world.v[5] = 2.5f;
        entity.world.v[12]                    = i * 8.0f;
        entity.world.v[14]                    = 0.5f;
        ViewDesc view;
        view.scene           = scene;
        view.output          = viewport;
        view.view.v[12]      = -static_cast<float>(i) * 8.0f;
        view.projection.v[0] = view.projection.v[5] = .5f;
        view.cameraPosition                         = {i * 8.0f, 0, i * 4.0f};
        packet.commands.emplace_back(CreateRenderEntity{{scene, {i, 1}}, entity});
        packet.commands.emplace_back(CreateView{{i, 1}, view});
    }
    ASSERT_TRUE(store.Apply(packet).Succeeded());
    catalog  = std::move(std::get<std::unique_ptr<ResourceCatalog>>(ResourceCatalog::Create({epoch})));
    uploader = std::move(std::get<std::unique_ptr<ResourceUploader>>(ResourceUploader::Create(*device, *context, *catalog)));
    std::vector<ResourceTicket> tickets;
    for (uint32_t i = 0; i != 2; ++i) {
        for (const auto slot : {ResourceBindingSlot::MESH, ResourceBindingSlot::MATERIAL}) {
            auto result = std::get<ResourceRequest>(catalog->Request(store, {{scene, {i, 1}}, slot, 1, 1}));
            if (!result.needsCpuData)
                continue;
            tickets.push_back(result.ticket);
            RenderResourcePayload payload{result.ticket.resource, Name("prepared-v1"), std::vector<uint8_t>(72), {}};
            if (slot == ResourceBindingSlot::MESH) {
                const float    positions[] = {-0.8f, -0.8f, 0, -0.8f, 0.8f, 0, 0.8f, -0.8f, 0, 0.8f, 0.8f, 0};
                const uint16_t indices[]   = {0, 1, 2, 2, 1, 3};
                std::memcpy(payload.bytes.data(), positions, sizeof(positions));
                std::memcpy(payload.bytes.data() + 48, indices, sizeof(indices));
                std::memcpy(payload.bytes.data() + 60, indices, sizeof(indices));
                payload.metadata = {AABB{{-.8f, -.8f, 0}, {.8f, .8f, 0}}, {2}};
            }
            ASSERT_FALSE(catalog->Supply(result.ticket, payload));
        }
    }
    {
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        auto              batch = std::get<ResourceUploadBatch>(uploader->Prepare(lease, builder, tickets));
        Probe             probe;
        probe.graph    = std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
        auto recording = Record(lease, probe);
        ASSERT_TRUE(recording);
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
        Complete(1);
    }
    FramePacket update;
    update.sequence = 2;
    for (uint32_t i = 0; i != 2; ++i) {
        auto entity                   = std::get<CreateRenderEntity>(packet.commands[2 + i * 2]).desc;
        entity.mesh.resource.revision = 2;
        entity.mesh.bindingRevision   = 2;
        update.commands.emplace_back(UpdateRenderEntity{{scene, {i, 1}}, entity, 1});
    }
    ASSERT_TRUE(store.Apply(update).Succeeded());
    for (uint32_t i = 0; i != 2; ++i)
        ASSERT_TRUE(std::holds_alternative<ResourceRequest>(catalog->Request(store, {{scene, {i, 1}}, ResourceBindingSlot::MESH, 2, 2})));
    auto resources = catalog->Freeze(store);
    auto frame     = store.Freeze();
    auto program   = std::get<PreparedSceneProgram>(BuildPreparedSceneProgram(*frame, resources.cpu, 1));
    auto scheduler = ScenePrepareScheduler::Create({3});
    auto submitted =
        scheduler.scheduler->Submit(*frame, program.work.systems, std::move(program.work.kernel), program.work.partitions, program.work.outputs);
    ASSERT_TRUE(submitted.Accepted());
    auto prepared = std::get<std::shared_ptr<const PreparedFrame>>(CollectPreparedScene(program, submitted.group->Wait()));
    frame.reset();
    ASSERT_FALSE(store.IsFrozen());
    std::array<PreparedMeshLayout, 1> meshes;
    meshes[0] = {mesh, {{0, 48, 12}}, {{0, 0, 0, Format::F_RGB32, "POSITION", 0}}, {{48, 12, IndexType::U16, {6}}, {60, 12, IndexType::U16, {6}}}};
    std::array<PreparedMaterialLayout, 2> materials;
    for (uint32_t i = 0; i != 2; ++i) {
        auto &material                         = materials[i];
        material.resource                      = i ? blue : red;
        material.shader                        = shader;
        material.vertexMask                    = RenderVertexMask(meshes[0].streams, meshes[0].attributes).mask;
        const std::array<float, 4>        tint = i ? std::array<float, 4>{0, 0, 1, 1} : std::array<float, 4>{1, 0, 0, 1};
        const shared::MaterialBlockParams batch{{tint[0], tint[1], tint[2], tint[3]}, {.5f, 0, 0, 0}};
        material.parameters.set    = shared::MaterialBlockBindings::gMaterial.set;
        material.parameters.values = {{Name("gMaterial.tint"), MakeShaderData(batch.tint)},
                                      {Name("gMaterial.surface"), MakeShaderData(batch.surface)}};
    }
    const auto        lease = Begin();
    FrameGraphBuilder builder;
    FrameRasterPass   pass;
    pass.color                   = ImageOf(builder, false);
    const auto color             = pass.color;
    auto       retained          = std::make_shared<std::vector<PreparedRasterView>>();
    auto       wrongMaterials    = materials;
    wrongMaterials[0].vertexMask = 0;
    EXPECT_TRUE(std::holds_alternative<FrameError>(
        ResolvePreparedRaster(*device, *context, lease, builder, prepared->views[0], resources, meshes, wrongMaterials, {})));
    auto       wrongMeshes           = meshes;
    wrongMeshes[0].resource.revision = 2;
    EXPECT_TRUE(std::holds_alternative<FrameError>(
        ResolvePreparedRaster(*device, *context, lease, builder, prepared->views[0], resources, wrongMeshes, materials, {})));
    wrongMeshes                      = meshes;
    wrongMeshes[0].streams[0].offset = UINT64_MAX;
    EXPECT_TRUE(std::holds_alternative<FrameError>(
        ResolvePreparedRaster(*device, *context, lease, builder, prepared->views[0], resources, wrongMeshes, materials, {})));
    wrongMeshes                 = meshes;
    wrongMeshes[0].lods[0].size = UINT64_MAX;
    EXPECT_TRUE(std::holds_alternative<FrameError>(
        ResolvePreparedRaster(*device, *context, lease, builder, prepared->views[0], resources, wrongMeshes, materials, {})));
    for (uint32_t i = 0; i != 2; ++i) {
        ASSERT_EQ(prepared->views[i].draws.size(), 1u);
        EXPECT_EQ(prepared->views[i].draws[0].mesh.resource, mesh);
        EXPECT_EQ(prepared->views[i].draws[0].lod, i);
        auto raster =
            ResolvePreparedRaster(*device, *context, lease, builder, prepared->views[i], resources, meshes, materials, {{int32_t(i * 4), 0}, {4, 8}});
        if (auto *error = std::get_if<FrameError>(&raster))
            FAIL() << error->message;
        retained->push_back(std::move(std::get<PreparedRasterView>(raster)));
        auto &item = retained->back().draws[0];
        EXPECT_EQ(item.draw.indexed->range.offset, i ? 60u : 48u);
        EXPECT_TRUE(item.draw.uniforms.empty());
        const auto                    &view = prepared->views[i].view;
        const shared::OpaquePassParams params{view.projection * view.view, {view.cameraPosition.x, view.cameraPosition.y, view.cameraPosition.z, 1}};
        const shared::SceneBatchParams batch{item.world};
        ASSERT_EQ(item.material.values.size(), 2u);
        FrameBuildInputs inputs;
        inputs.lease = lease;
        FrameComposer        composer(*device, *context, builder, inputs);
        PassResourceBindings passValues(shared::GetOpaquePassReflection());
        PassResourceBindings batchValues(shared::GetSceneBatchReflection());
        PassResourceBindings shaderValues(shared::GetMaterialBlockReflection());
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(passValues.UniformData(shared::OpaquePassBindings::gOpaquePass, params)));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(
            batchValues.UniformData(shared::SceneBatchBindings::gSceneBatch, batch, ShaderResourceType::UNIFORM_BUFFER_DYNAMIC)));
        for (const auto &value : item.material.values)
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(shaderValues.Data(value.name, value.value)));
        const PassResourceBindings *groups[]{&passValues, &batchValues, &shaderValues};
        const auto                  applied = PassResourceBindings::Apply(composer, item.draw, groups);
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(applied));
        pass.draws.push_back(std::move(item.draw));
    }
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(pass))));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.KeepAlive(retained)));
    Buffer::Descriptor desc;
    desc.size   = 256;
    desc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    desc.memory = GetParam() == 2 ? MemoryType::GPU_TO_CPU : MemoryType::CPU_ONLY;
    Probe probe;
    probe.readback         = BufferPtr(device->CreateBuffer(desc));
    const auto      target = Import(builder, probe.readback, CopyBufferState::HOST_READ, false);
    BufferImageCopy region;
    region.imageExtent = {8, 8, 1};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({target, color, region, ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({target, 0, 256})));
    probe.graph = std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
    resources   = {};
    prepared.reset();
    retained.reset();
    uploader.reset();
    catalog.reset();
    auto recording = Record(lease, probe);
    ASSERT_TRUE(recording);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete(1);
    Pixels(probe);
}

INSTANTIATE_TEST_SUITE_P(Backends, IndexedRaster, testing::Values(0u, 1u, 2u));

TEST_P(IndexedRaster, SelectedColorAndDepthMipLayersMatchExtentsAndParallelDraws)
{
    for (const auto workers : {1u, 3u}) {
        for (const auto layers : {1u, 2u}) {
            info.parallelNum = workers;
            ResetContext();
            const auto lease = Begin();
            auto       probe = MakeGraph(lease, IndexType::U16, 70, false, true);
            auto       data  = probe.graph->GetData();
            for (auto &image : data.images) {
                const bool depth             = image.descriptor.format == PixelFormat::D32;
                image.descriptor.extent      = depth ? Extent3D{32, 32, 1} : Extent3D{16, 16, 1};
                image.descriptor.mipLevels   = depth ? 3 : 2;
                image.descriptor.arrayLayers = layers;
                image.image                  = ImagePtr(device->CreateImage(image.descriptor));
                ASSERT_TRUE(image.image);
            }
            for (auto &operation : data.passes) {
                if (auto *pass = std::get_if<FrameRasterPass>(&operation)) {
                    pass->colorView   = {1, layers - 1};
                    pass->depth->view = {2, layers - 1};
                } else if (auto *copy = std::get_if<BufferImageCopyPass>(&operation)) {
                    copy->region.subRange.level     = 1;
                    copy->region.subRange.baseLayer = layers - 1;
                }
            }
            for (uint32_t invalid = 0; invalid < 3; ++invalid) {
                auto  bad  = data;
                auto &pass = std::get<FrameRasterPass>(bad.passes[1]);
                if (invalid == 0)
                    pass.depth->view.mip = 1;
                else if (invalid == 1)
                    pass.depth->view.layer = 2;
                else
                    pass.draws[0].scissor.extent.width = 9;
                EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(probe.graph->GetId(), std::move(bad)))));
            }
            probe.graph    = std::make_shared<const FrozenFrameGraph>(probe.graph->GetId(), std::move(data));
            auto recording = Record(lease, probe);
            ASSERT_TRUE(recording);
            ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
            Complete(1);
            Pixels(probe);
        }
    }
}

TEST_P(IndexedRaster, MultisamplePipelineAndResolveFollowLastParallelFragment)
{
    for (const auto workers : {1u, 3u})
        for (const auto layers : {1u, 2u}) {
            info.parallelNum = workers;
            ResetContext();
            const auto lease              = Begin();
            auto       probe              = MakeGraph(lease, IndexType::U16, 70, false, true);
            auto       data               = probe.graph->GetData();
            auto      &source             = data.images[0];
            source.image                  = {};
            source.transient              = true;
            source.descriptor.samples     = SampleCount::X4;
            source.descriptor.arrayLayers = layers;
            source.descriptor.usage       = ImageUsageFlagBit::RENDER_TARGET;
            source.final                  = ImageLayout::GENERAL;
            FrameGraphImage resolved;
            resolved.descriptor.format      = PixelFormat::RGBA8_UNORM;
            resolved.descriptor.extent      = {16, 16, 1};
            resolved.descriptor.mipLevels   = 2;
            resolved.descriptor.arrayLayers = layers;
            resolved.descriptor.usage       = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
            resolved.image                  = ImagePtr(device->CreateImage(resolved.descriptor));
            ASSERT_TRUE(resolved.image);
            const GraphImageHandle target{probe.graph->GetId(), static_cast<uint32_t>(data.images.size())};
            data.images.push_back(std::move(resolved));
            for (auto &operation : data.passes) {
                if (auto *pass = std::get_if<FrameRasterPass>(&operation)) {
                    pass->colorView = {0, layers - 1};
                    pass->depth.reset();
                    pass->colorResolve = RasterColorResolve{target, {1, pass->load == LoadOp::LOAD ? layers - 1 : 0}};
                    if (pass->load == LoadOp::LOAD)
                        pass->store = StoreOp::DONT_CARE;
                    for (auto &draw : pass->draws) {
                        draw.depth = {};
                        if (pass->load == LoadOp::LOAD)
                            draw.scissor = {{4, 0}, {4, 8}};
                    }
                } else if (auto *copy = std::get_if<BufferImageCopyPass>(&operation)) {
                    copy->image                     = target;
                    copy->region.subRange.level     = 1;
                    copy->region.subRange.baseLayer = layers - 1;
                }
            }
            if (layers == 2) {
                auto               copy   = std::get<BufferImageCopyPass>(data.passes.back());
                auto              &output = data.buffers[copy.buffer.index];
                Buffer::Descriptor desc;
                desc.size      = 512;
                desc.usage     = BufferUsageFlagBit::TRANSFER_DST;
                desc.memory    = GetParam() == 2 ? MemoryType::GPU_TO_CPU : MemoryType::CPU_ONLY;
                probe.readback = BufferPtr(device->CreateBuffer(desc));
                ASSERT_TRUE(probe.readback);
                output.buffer                  = probe.readback;
                output.size                    = desc.size;
                copy.region.subRange.baseLayer = 0;
                copy.region.bufferOffset       = 256;
                data.passes.emplace_back(copy);
                data.exports.push_back({copy.buffer, 256, 256});
            }
            probe.graph    = std::make_shared<const FrozenFrameGraph>(probe.graph->GetId(), std::move(data));
            auto recording = Record(lease, probe);
            ASSERT_TRUE(recording);
            ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
            Complete(1);
            Pixels(probe);
            if (layers == 2) {
                const auto *bytes = probe.readback->Map();
                ASSERT_TRUE(bytes);
                for (uint32_t y = 0; y < 8; ++y)
                    for (uint32_t x = 0; x < 8; ++x) {
                        const auto offset = 256 + (y * 8 + x) * 4;
                        EXPECT_EQ(bytes[offset], x < 4 ? 255 : 0);
                        EXPECT_EQ(bytes[offset + 1], x < 4 ? 0 : 255);
                        EXPECT_EQ(bytes[offset + 2], 0);
                        EXPECT_EQ(bytes[offset + 3], 255);
                    }
                probe.readback->UnMap();
            }
        }
}

TEST_P(IndexedRaster, MultisampleDepthLoadAndLayerSelectionMatchSerialAndParallel)
{
    for (const bool resolveEachPass : {false, true})
        for (const auto samples : {SampleCount::X2, SampleCount::X4, SampleCount::X8})
            for (const auto workers : {1u, 3u})
                for (const auto layers : {1u, 2u}) {
                    SCOPED_TRACE(testing::Message() << resolveEachPass << "/" << static_cast<uint32_t>(samples) << "/" << workers << "/" << layers);
                    info.parallelNum = workers;
                    ResetContext();
                    const auto lease = Begin();
                    auto       probe = MakeGraph(lease, IndexType::U16, 70, false, true);
                    auto       data  = probe.graph->GetData();
                    for (uint32_t i = 0; i < 2; ++i) {
                        auto &image                  = data.images[i];
                        image.image                  = {};
                        image.transient              = true;
                        image.descriptor.samples     = samples;
                        image.descriptor.arrayLayers = layers;
                        image.descriptor.usage       = i ? ImageUsageFlagBit::DEPTH_STENCIL : ImageUsageFlagBit::RENDER_TARGET;
                        image.final                  = ImageLayout::GENERAL;
                    }
                    FrameGraphImage resolved;
                    resolved.descriptor.format      = PixelFormat::RGBA8_UNORM;
                    resolved.descriptor.extent      = {16, 16, 1};
                    resolved.descriptor.mipLevels   = 2;
                    resolved.descriptor.arrayLayers = layers;
                    resolved.descriptor.usage       = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
                    resolved.image                  = ImagePtr(device->CreateImage(resolved.descriptor));
                    ASSERT_TRUE(resolved.image);
                    const GraphImageHandle target{probe.graph->GetId(), static_cast<uint32_t>(data.images.size())};
                    data.images.push_back(std::move(resolved));
                    for (auto &operation : data.passes) {
                        if (auto *pass = std::get_if<FrameRasterPass>(&operation)) {
                            pass->colorView = pass->depth->view = {0, layers - 1};
                            pass->colorResolve                  = RasterColorResolve{target, {1, layers - 1}};
                            if (pass->load == LoadOp::LOAD)
                                pass->store = pass->depth->store = StoreOp::DONT_CARE;
                        } else if (auto *copy = std::get_if<BufferImageCopyPass>(&operation)) {
                            copy->image                     = target;
                            copy->region.subRange.level     = 1;
                            copy->region.subRange.baseLayer = layers - 1;
                        }
                    }
                    if (!resolveEachPass) {
                        auto &first  = std::get<FrameRasterPass>(data.passes[1]);
                        auto &last   = std::get<FrameRasterPass>(data.passes[2]);
                        auto  finish = last;
                        finish.draws.clear();
                        first.colorResolve.reset();
                        last.colorResolve.reset();
                        last.store = last.depth->store = StoreOp::STORE;
                        data.passes.insert(data.passes.begin() + 3, std::move(finish));
                    }
                    if (layers == 2) {
                        // Clearing another cell between STORE and LOAD must not overwrite the selected depth.
                        auto poison = std::get<FrameRasterPass>(data.passes[1]);
                        poison.draws.clear();
                        poison.colorResolve.reset();
                        poison.colorView = poison.depth->view = {0, 0};
                        poison.depth->clear                   = 0.f;
                        poison.sideEffect                     = true;
                        data.passes.insert(data.passes.begin() + 2, std::move(poison));
                    }
                    probe.graph = std::make_shared<const FrozenFrameGraph>(probe.graph->GetId(), std::move(data));
                    PipelineProbe pipelines(*device);
                    afterCompile   = [&] { pipelines.frozen = true; };
                    auto recording = Record(lease, probe);
                    afterCompile   = {};
                    ASSERT_TRUE(recording);
                    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
                    Complete(1);
                    Pixels(probe);
                    EXPECT_LE(pipelines.pipelines.load(), 4u);
                    EXPECT_EQ(pipelines.lateWrites.load(), 0u);
                }
}


TEST(VertexInputResolution, ReordersBySemanticAndRejectsMissingDuplicateOrWrongNumericType)
{
    std::vector<RasterVertexAttribute> attributes{
        {0, 0, 0, Format::F_RGB32, "POSITION", 0}, {1, 0, 12, Format::F_RGB32, "NORMAL", 0},
        {2, 0, 24, Format::U_RGBA8, "JOINTS", 0}, {3, 0, 28, Format::F_RGBA32, "WEIGHTS", 0}};
    ShaderReflection reflection;
    reflection.vertexInputs = {{"POSITION", 0, 0, ShaderScalarType::FLOAT, 3},
                               {"JOINTS", 0, 1, ShaderScalarType::UINT, 4}, {"WEIGHTS", 0, 2, ShaderScalarType::FLOAT, 4}};
    const auto resolved = ResolveVertexInputs(attributes, reflection);
    ASSERT_TRUE(std::holds_alternative<std::vector<RasterVertexAttribute>>(resolved));
    const auto &inputs = std::get<std::vector<RasterVertexAttribute>>(resolved);
    ASSERT_EQ(inputs.size(), 3u);
    EXPECT_EQ(inputs[1].location, 1u);
    EXPECT_EQ(inputs[1].offset, 24u);
    EXPECT_EQ(inputs[1].format, Format::U_RGBA8);
    attributes[2].format = Format::F_RGBA32;
    EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveVertexInputs(attributes, reflection)));
    attributes[2].format = Format::U_RGBA8;
    attributes.push_back(attributes[2]);
    EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveVertexInputs(attributes, reflection)));
    attributes.pop_back();
    attributes.pop_back();
    EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveVertexInputs(attributes, reflection)));
}

TEST_P(IndexedRaster, SkeletalStorageUsesIntegerSlotsExactRangesAndTwoInflightPoses)
{
    ShaderCompilerSlang compiler;
    auto *previous = ShaderCompilerFactory::Get().GetCompiler();
    ShaderCompilerFactory::Get().Register(&compiler);
    Finally restore{[&] { ShaderCompilerFactory::Get().Register(previous); }};
    ShaderFileSystem files;
    ASSERT_TRUE(LoadTestShaderSources(files));
    files.AddVirtualFile("SkinReadback.slang", R"(
        #include <aurora/materials/Skinning.slangh>
        struct Varying { float4 position : SV_Position; float3 color : COLOR0; };
        Varying VSMain(float3 p : POSITION, uint4 joints : JOINTS, float4 weights : WEIGHTS) {
            Varying result;
            result.position = float4(SkinPosition(p, joints, weights), 1);
            float3 normal = SkinNormal(float3(1, 0, 0), joints, weights);
            float4 tangent = SkinTangent(float4(0, 1, 0, 1), normal, joints, weights);
            result.color = float3(normal.x * 0.5 + 0.5, tangent.y, 0.25);
            return result;
        }
        float4 FSMain(Varying input) : SV_Target { return float4(input.color, 1); }
    )");
    ShaderResolver resolver(nullptr, nullptr, files, 1, 1);
    ShaderVariantSchema schema;
    RasterShaderRequest request;
    request.vertex.relativePath = "SkinReadback.slang";
    request.vertex.entry = "VSMain";
    request.vertex.stage = ShaderStageFlagBit::VS;
    request.vertex.target = static_cast<uint32_t>(GetParam() == 2 ? ShaderTarget::DXIL : ShaderTarget::SPIRV);
    request.vertex.schema = &schema;
    request.vertex.schemaFp = HashSchema(schema);
    request.fragment = request.vertex;
    request.fragment.entry = "FSMain";
    request.fragment.stage = ShaderStageFlagBit::FS;
    const auto created = ResolveRasterShader(*device, resolver, request);
    if (const auto *error = std::get_if<FrameError>(&created)) { FAIL() << error->message; }
    const auto shader = std::get<ShaderPtr>(created);
    ASSERT_EQ(shader->GetReflection().resources.size(), 1u);
    EXPECT_EQ(shader->GetReflection().resources[0].elementStride, 16u);
    EXPECT_EQ(shader->GetReflection().resources[0].bufferKind, ShaderBufferKind::STRUCTURED);
    EXPECT_EQ(shader->GetReflection().resources[0].access, ShaderResourceAccess::READ_ONLY);
    struct Vertex { Vector3 position; Vector3 unusedNormal{0, 0, 1}; std::array<uint8_t, 4> joints{0, 1, 0, 0}; Vector4 weights{.25f, .75f, 0, 0}; };
    const std::array<Vertex, 6> vertices{{{{-1, -1, .5f}}, {{1, -1, .5f}}, {{-1, 1, .5f}}, {{-1, 1, .5f}}, {{1, -1, .5f}}, {{1, 1, .5f}}}};
    const auto make = [&](DeviceFrameLease lease, bool flipped) {
        Probe probe;
        FrameGraphBuilder builder;
        FrameBuildInputs inputs;
        inputs.lease = lease;
        FrameComposer composer(*device, *context, builder, inputs);
        const auto vertexData = composer.Upload(std::as_bytes(std::span{vertices}), BufferUsageFlagBit::VERTEX);
        EXPECT_TRUE(std::holds_alternative<RasterBufferRange>(vertexData));
        if (!std::holds_alternative<RasterBufferRange>(vertexData)) { return probe; }
        const auto color = ImageOf(builder, false);
        FrameRasterPass pass;
        pass.color = color;
        for (uint32_t instance = 0; instance < 2; ++instance) {
            std::array<Matrix4, 4> palette;
            for (uint32_t joint = 0; joint < 2; ++joint) {
                auto &matrix = palette[joint * 2];
                matrix = Matrix4::Identity();
                matrix[0].x = ((instance != 0) != flipped) ? -.5f : .5f;
                matrix[3].x = (instance ? 1.f : -1.f) * (joint ? .6f : .2f);
                palette[joint * 2 + 1] = matrix.InverseTranspose();
            }
            const auto storage = composer.Upload(std::as_bytes(std::span{palette}), BufferUsageFlagBit::STORAGE);
            EXPECT_TRUE(std::holds_alternative<RasterBufferRange>(storage));
            if (!std::holds_alternative<RasterBufferRange>(storage)) { return probe; }
            const auto range = std::get<RasterBufferRange>(storage);
            EXPECT_EQ(range.size, sizeof(palette));
            RasterDraw draw;
            draw.shader = shader;
            draw.command = {6, 1, 0, 0};
            draw.scissor = {{0, 0}, {8, 8}};
            draw.vertices = {{std::get<RasterBufferRange>(vertexData), sizeof(Vertex)}};
            draw.attributes = {{0, 0, offsetof(Vertex, position), Format::F_RGB32, "POSITION", 0},
                               {1, 0, offsetof(Vertex, unusedNormal), Format::F_RGB32, "NORMAL", 0},
                               {2, 0, offsetof(Vertex, joints), Format::U_RGBA8, "JOINTS", 0},
                               {3, 0, offsetof(Vertex, weights), Format::F_RGBA32, "WEIGHTS", 0}};
            auto resolved = ResolveVertexInputs(draw.attributes, shader->GetReflection());
            EXPECT_TRUE(std::holds_alternative<std::vector<RasterVertexAttribute>>(resolved));
            if (const auto *error = std::get_if<FrameError>(&resolved)) { ADD_FAILURE() << error->message; return probe; }
            draw.attributes = std::move(std::get<std::vector<RasterVertexAttribute>>(resolved));
            PassResourceBindings bindings(shared::GetSkinningBatchReflection());
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(bindings.Storage(shared::SkinningBatchBindings::gSkinningPalette, range)));
            const PassResourceBindings *groups[]{&bindings};
            const auto                  applied = PassResourceBindings::Apply(composer, draw, groups);
            if (const auto *applyError = std::get_if<FrameError>(&applied)) { ADD_FAILURE() << applyError->message; return probe; }
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(applied));
            pass.draws.push_back(std::move(draw));
        }
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(pass))));
        Buffer::Descriptor readback;
        readback.size = 8 * 8 * 4;
        readback.usage = BufferUsageFlagBit::TRANSFER_DST;
        readback.memory = GetParam() == 2 ? MemoryType::GPU_TO_CPU : MemoryType::CPU_ONLY;
        probe.readback = BufferPtr(device->CreateBuffer(readback));
        const auto outputBuffer = Import(builder, probe.readback, CopyBufferState::HOST_READ, false);
        BufferImageCopy region;
        region.imageExtent = {8, 8, 1};
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({outputBuffer, color, region, ImageCopyDirection::IMAGE_TO_BUFFER})));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({outputBuffer, 0, readback.size})));
        const auto frozen = builder.Freeze();
        EXPECT_TRUE(std::holds_alternative<std::shared_ptr<const FrozenFrameGraph>>(frozen));
        if (const auto *graph = std::get_if<std::shared_ptr<const FrozenFrameGraph>>(&frozen)) { probe.graph = *graph; }
        return probe;
    };
    const auto firstLease = Begin();
    const auto first = make(firstLease, false);
    ASSERT_NE(first.graph, nullptr);
    auto firstRecording = Record(firstLease, first);
    ASSERT_NE(firstRecording, nullptr);
    const std::array<Matrix4, 2> overwrite{Matrix4::Identity(), Matrix4::Identity()};
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->WriteFrameBuffer(firstLease, FrameBufferKind::STORAGE, std::as_bytes(std::span{overwrite}), 64)));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(firstRecording)));
    const auto secondLease = Begin();
    const auto second = make(secondLease, true);
    ASSERT_NE(second.graph, nullptr);
    auto secondRecording = Record(secondLease, second);
    ASSERT_NE(secondRecording, nullptr);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(secondRecording)));
    Complete(2);
    for (uint32_t frame = 0; frame < 2; ++frame) {
        const auto &probe = frame ? second : first;
        auto *pixels = probe.readback->Map();
        ASSERT_NE(pixels, nullptr);
        for (uint32_t y = 0; y < 8; ++y) {
            for (uint32_t x = 0; x < 8; ++x) {
                const auto offset = (y * 8 + x) * 4;
                EXPECT_NEAR(pixels[offset], ((x < 4) != (frame != 0)) ? 255 : 0, 1) << frame << ":" << x << "," << y;
                EXPECT_NEAR(pixels[offset + 1], 255, 1);
                EXPECT_NEAR(pixels[offset + 2], 64, 1);
                EXPECT_EQ(pixels[offset + 3], 255);
            }
        }
        probe.readback->UnMap();
    }
}

TEST(MaterialVariantSelection, SelectsExactPipelineAndVertexVariant)
{
    const RenderResourceKey key{Uuid::CreateWithSeed(9901), 1, 1, 1};
    std::array<PreparedMaterialLayout, 3> layouts;
    layouts[0].resource = key;
    layouts[1].resource = key;
    layouts[1].pipeline.entries.push_back({Name("SHADOWS"), 1});
    layouts[2].resource = key;
    layouts[2].skinned  = true;

    ShaderVariant shadow;
    shadow.entries.push_back({Name("SHADOWS"), 1});
    ShaderVariant environment;
    environment.entries.push_back({Name("IBL"), 1});

    EXPECT_EQ(FindMaterialVariant(layouts, key, false, {}), &layouts[0]);
    EXPECT_EQ(FindMaterialVariant(layouts, key, false, shadow), &layouts[1]);
    EXPECT_EQ(FindMaterialVariant(layouts, key, true, {}), &layouts[2]);
    EXPECT_EQ(FindMaterialVariant(layouts, key, false, environment), nullptr);
    EXPECT_EQ(FindMaterialVariant(layouts, key, true, shadow), nullptr);

    PreparedMaterialLayout legacy;
    legacy.resource         = {Uuid::CreateWithSeed(9902), 1, 1, 1};
    legacy.pipelineAgnostic = true;
    const std::array<PreparedMaterialLayout, 1> legacyLayouts{legacy};
    EXPECT_EQ(FindMaterialVariant(legacyLayouts, legacy.resource, false, environment), &legacyLayouts[0]);

    // Old and new material revisions coexist by exact resource identity; a
    // frozen draw never picks the other generation's program.
    std::array<PreparedMaterialLayout, 2> generations;
    generations[0].resource = {Uuid::CreateWithSeed(9903), 1, 1, 1};
    generations[1].resource = {Uuid::CreateWithSeed(9903), 2, 1, 1};
    EXPECT_EQ(FindMaterialVariant(generations, generations[0].resource, false, {}), &generations[0]);
    EXPECT_EQ(FindMaterialVariant(generations, generations[1].resource, false, {}), &generations[1]);
    EXPECT_EQ(FindMaterialVariant(generations, {Uuid::CreateWithSeed(9903), 3, 1, 1}, false, {}), nullptr);
}
