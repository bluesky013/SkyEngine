//
// Created on 2026/10/16.
//

#include "GpuTestSupport.h"

#include <aurora/rdg/FrameBindings.h>

#include "graph/FrameGraphBuilder.h"
#include "graph/RasterShaderResolver.h"
#include "resource/ResourceUploader.h"
#include <D3D12Device.h>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <d3d12sdklayers.h>
#include <set>

namespace {
    template <class Edit>
    FrameOperationResult EditBindings(RasterDraw &draw, Edit edit)
    {
        auto bindings = ResolveRasterBindings(draw);
        edit(bindings);
        return SetRasterBindings(draw, std::move(bindings));
    }
    constexpr SceneId  scene{0, 1};
    constexpr uint64_t epoch = 113;
    RenderResourceKey  Key(uint64_t revision = 1)
    {
        return {Uuid::CreateWithSeed(113), revision, 1, epoch};
    }
    ResourceSubscriber Subscriber(uint32_t index = 0, uint64_t revision = 1, uint64_t sequence = 1)
    {
        return {{scene, {index, 1}}, ResourceBindingSlot::MESH, revision, sequence};
    }
    RenderEntityDesc Desc(uint64_t version, uint64_t binding = 1)
    {
        RenderEntityDesc desc;
        desc.mesh = {Key(version), binding};
        return desc;
    }
    class UnissuedRecording final : public RecordedFrame {
    public:
        explicit UnissuedRecording(const RecordedFrame &source) : identity(source.GetIdentity()), receipt(*source.GetGraphReceipt())
        {
        }
        CompiledFrameIdentity GetIdentity() const noexcept override
        {
            return identity;
        }
        const FrameGraphReceipt *GetGraphReceipt() const noexcept override
        {
            return &receipt;
        }
        CompiledFrameIdentity identity;
        FrameGraphReceipt     receipt;
    };
    struct UploadFaults {
        explicit UploadFaults(Device &device)
        {
            if (device.GetAPI() == API::VULKAN) {
                vk      = &const_cast<VulkanDeviceFunctions &>(static_cast<VulkanDevice &>(device).GetDeviceFn());
                savedVk = *vk;
            } else {
                dx      = &const_cast<D3D12Functions &>(static_cast<D3D12Device &>(device).GetFunctions());
                savedDx = *dx;
            }
        }
        ~UploadFaults()
        {
            Restore();
        }
        void Restore()
        {
            if (vk)
                *vk = savedVk;
            if (dx)
                *dx = savedDx;
            activeQuery = nullptr;
        }
        void FailEncoding()
        {
            if (vk)
                vk->vkEndCommandBuffer = [](VkCommandBuffer) -> VkResult { return VK_ERROR_OUT_OF_HOST_MEMORY; };
            if (dx)
                dx->closeList = [](ID3D12GraphicsCommandList *) -> HRESULT { return E_OUTOFMEMORY; };
        }
        void UncertainSubmit()
        {
            if (vk)
                vk->vkQueueSubmit2 = [](VkQueue, uint32_t, const VkSubmitInfo2 *, VkFence) -> VkResult { return VK_ERROR_UNKNOWN; };
            if (dx)
                dx->signal = [](ID3D12CommandQueue *, ID3D12Fence *, UINT64) -> HRESULT { return E_OUTOFMEMORY; };
        }
        void FailQuery()
        {
            if (vk)
                vk->vkGetFenceStatus = [](VkDevice, VkFence) -> VkResult { return VK_ERROR_DEVICE_LOST; };
            if (dx)
                dx->completed = [](ID3D12Fence *) -> UINT64 { return UINT64_MAX; };
        }
        void FailSecondQuery()
        {
            activeQuery = this;
            if (vk)
                vk->vkGetFenceStatus = [](VkDevice device, VkFence fence) -> VkResult {
                    return ++activeQuery->queries == 2 ? VK_ERROR_DEVICE_LOST : activeQuery->savedVk.vkGetFenceStatus(device, fence);
                };
            if (dx)
                dx->completed = [](ID3D12Fence *fence) -> UINT64 {
                    return ++activeQuery->queries == 2 ? UINT64_MAX : activeQuery->savedDx.completed(fence);
                };
        }
        inline static UploadFaults *activeQuery = nullptr;
        uint32_t                    queries     = 0;
        VulkanDeviceFunctions      *vk          = nullptr;
        VulkanDeviceFunctions       savedVk{};
        D3D12Functions             *dx = nullptr;
        D3D12Functions              savedDx;
    };
    struct SampledViewProbe {
        explicit SampledViewProbe(VulkanDevice &device) : functions(const_cast<VulkanDeviceFunctions &>(device.GetDeviceFn())), saved(functions)
        {
            active                       = this;
            functions.vkCreateImageView  = Create;
            functions.vkDestroyImageView = Destroy;
        }
        ~SampledViewProbe()
        {
            functions = saved;
            active    = nullptr;
        }
        static VkResult VKAPI_PTR Create(VkDevice device, const VkImageViewCreateInfo *info, const VkAllocationCallbacks *allocator, VkImageView *out)
        {
            const bool sampled = info->subresourceRange.baseMipLevel > 0;
            if (sampled) {
                ++active->attempts;
                if (active->frozen)
                    ++active->lateCreates;
                if (active->failAt == active->attempts)
                    return VK_ERROR_OUT_OF_HOST_MEMORY;
            }
            const auto result = active->saved.vkCreateImageView(device, info, allocator, out);
            if (sampled && result == VK_SUCCESS)
                active->live.insert(*out);
            return result;
        }
        static void VKAPI_PTR Destroy(VkDevice device, VkImageView view, const VkAllocationCallbacks *allocator)
        {
            if (active->live.erase(view))
                ++active->destroyed;
            active->saved.vkDestroyImageView(device, view, allocator);
        }
        inline static SampledViewProbe *active = nullptr;
        VulkanDeviceFunctions          &functions;
        VulkanDeviceFunctions           saved;
        std::set<VkImageView>           live;
        uint32_t                        attempts = 0, failAt = 0, destroyed = 0, lateCreates = 0;
        bool                            frozen = false;
    };
    class ResourceUploads : public testing::TestWithParam<uint32_t> {
    protected:
        void SetUp() override
        {
            Instance::Get()->Init({"ResourceUploadTest", "SkyEngine", true, GetParam() % 3 == 2 ? API::DX12 : API::VULKAN});
            device = Instance::Get()->GetDevice();
            ASSERT_TRUE(device);
            if (GetParam() % 3 == 2) {
                ASSERT_TRUE(SUCCEEDED(static_cast<D3D12Device *>(device)->GetNativeHandle()->QueryInterface(IID_PPV_ARGS(&debug))));
                debug->ClearStoredMessages();
            }
            info.deviceEpoch              = epoch;
            info.parallelNum              = 2;
            info.vulkanCompiler           = GetParam() % 3 == 1 ? VulkanFrameGraphMode::RENDER_PASS : VulkanFrameGraphMode::DYNAMIC_RENDERING;
            info.allocations.stagingBytes = 256;
            context.reset(device->CreateFrameContext(info));
            ASSERT_TRUE(context);
            auto created = ResourceCatalog::Create({epoch});
            ASSERT_TRUE(std::holds_alternative<std::unique_ptr<ResourceCatalog>>(created));
            catalog = std::move(std::get<std::unique_ptr<ResourceCatalog>>(created));
            ResetUploader();
            Apply({CreateScene{scene, {}}, CreateRenderEntity{Subscriber().entity, Desc(1)}, CreateRenderEntity{Subscriber(1).entity, Desc(2)}});
        }
        void TearDown() override
        {
            uploader.reset();
            catalog.reset();
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
        void ResetUploader(ResourceUploadLimits limits = {})
        {
            uploader.reset();
            auto created = ResourceUploader::Create(*device, *context, *catalog, limits);
            ASSERT_TRUE(std::holds_alternative<std::unique_ptr<ResourceUploader>>(created));
            uploader = std::move(std::get<std::unique_ptr<ResourceUploader>>(created));
        }
        void Apply(std::vector<RenderCommand> commands)
        {
            FramePacket packet;
            packet.sequence   = ++packetSequence;
            packet.commands   = std::move(commands);
            const auto result = store.Apply(packet);
            ASSERT_TRUE(result.Succeeded());
            ASSERT_TRUE(result.skipped.empty());
        }
        ResourceTicket Supply(uint32_t                         entity   = 0,
                              uint64_t                         revision = 1,
                              uint64_t                         binding  = 1,
                              uint64_t                         sequence = 1,
                              bool                             retry    = false,
                              std::optional<RenderImageLayout> image    = {})
        {
            const auto  result  = catalog->Request(store, Subscriber(entity, binding, sequence), retry);
            const auto *request = std::get_if<ResourceRequest>(&result);
            EXPECT_TRUE(request);
            if (!request)
                return {};
            RenderResourcePayload payload{Key(revision),
                                          Name("buffer-v1"),
                                          std::vector<uint8_t>(64, static_cast<uint8_t>(revision)),
                                          {AABB{Vector3(-static_cast<float>(revision)), Vector3(static_cast<float>(revision))}}};
            if (GetParam() >= 3 || image) {
                payload.format         = Name("image-v1");
                payload.metadata.image = image.value_or(RenderImageLayout{4, 4});
                payload.bytes          = ImageBytes(*payload.metadata.image, static_cast<uint8_t>(revision));
            }
            EXPECT_FALSE(catalog->Supply(request->ticket, payload));
            return request->ticket;
        }
        ResourceCatalogStatus Status(const ResourceTicket &ticket)
        {
            const auto result = catalog->GetStatus(ticket);
            EXPECT_TRUE(std::holds_alternative<ResourceCatalogStatus>(result));
            const auto *value = std::get_if<ResourceCatalogStatus>(&result);
            return value ? *value : ResourceCatalogStatus{};
        }
        DeviceFrameLease Begin()
        {
            auto        result = context->TryBeginSlot();
            const auto *lease  = std::get_if<DeviceFrameLease>(&result);
            EXPECT_TRUE(lease);
            return lease ? *lease : DeviceFrameLease{};
        }
        ResourceUploadBatch Prepare(const DeviceFrameLease &lease, FrameGraphBuilder &builder, std::initializer_list<ResourceTicket> tickets)
        {
            auto result = uploader->Prepare(lease, builder, {tickets.begin(), tickets.size()});
            EXPECT_TRUE(std::holds_alternative<ResourceUploadBatch>(result));
            auto *batch = std::get_if<ResourceUploadBatch>(&result);
            return batch ? std::move(*batch) : ResourceUploadBatch{};
        }
        std::shared_ptr<const FrozenFrameGraph> Freeze(FrameGraphBuilder &builder)
        {
            auto result = builder.Freeze();
            EXPECT_TRUE(std::holds_alternative<std::shared_ptr<const FrozenFrameGraph>>(result));
            const auto *graph = std::get_if<std::shared_ptr<const FrozenFrameGraph>>(&result);
            return graph ? *graph : nullptr;
        }
        FrameCollectResult Collect(const EncodingTicket &ticket)
        {
            const auto deadline = std::chrono::steady_clock::now() + 5s;
            auto       result   = context->TryCollect(ticket);
            while (std::holds_alternative<FramePending>(result) && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(1ms);
                result = context->TryCollect(ticket);
            }
            return result;
        }
        std::unique_ptr<RecordedFrame> Record(const DeviceFrameLease &lease, std::shared_ptr<const FrozenFrameGraph> graph)
        {
            auto  compiled = context->Compile(lease, std::move(graph));
            auto *plan     = std::get_if<std::unique_ptr<BackendCompiledFrame>>(&compiled);
            if (const auto *error = std::get_if<FrameError>(&compiled))
                ADD_FAILURE() << "Compile " << static_cast<int>(error->code) << ": " << error->message;
            EXPECT_TRUE(plan);
            if (!plan)
                return {};
            auto        dispatched = context->Dispatch(*plan);
            const auto *ticket     = std::get_if<EncodingTicket>(&dispatched);
            EXPECT_TRUE(ticket);
            if (!ticket)
                return {};
            auto  collected = Collect(*ticket);
            auto *recording = std::get_if<std::unique_ptr<RecordedFrame>>(&collected);
            EXPECT_TRUE(recording);
            return recording ? std::move(*recording) : nullptr;
        }
        static std::vector<uint8_t> ImageBytes(const RenderImageLayout &layout, uint8_t version)
        {
            std::vector<uint8_t> bytes;
            for (uint32_t layer = 0; layer < layout.arrayLayers; ++layer)
                for (uint32_t mip = 0; mip < layout.mipLevels; ++mip)
                    bytes.insert(bytes.end(), size_t(std::max(1u, layout.width >> mip)) * std::max(1u, layout.height >> mip) * 4,
                                 static_cast<uint8_t>(version + layer * 13 + mip * 3));
            return bytes;
        }
        using Resolved = std::variant<FrameError, GraphBufferHandle, GraphImageHandle>;
        Resolved Import(FrameGraphBuilder &builder, const ResourceBacking &backing)
        {
            if (const auto *buffer = std::get_if<ResourceBufferBacking>(&backing)) {
                const auto imported = builder.Import(buffer->buffer, buffer->state, buffer->state, true);
                return std::visit([](const auto &value) -> Resolved { return value; }, imported);
            }
            const auto &image    = std::get<ResourceImageBacking>(backing);
            const auto  imported = builder.Import({image.image, image.descriptor, image.state, image.state, true});
            return std::visit([](const auto &value) -> Resolved { return value; }, imported);
        }
        BufferPtr Readback(FrameGraphBuilder &builder, Resolved source, bool exported = true, RenderImageLayout layout = {4, 4})
        {
            Buffer::Descriptor desc;
            desc.size   = std::holds_alternative<GraphImageHandle>(source) ? *RenderImageByteSize(layout) : 64;
            desc.memory = GetParam() % 3 == 2 ? MemoryType::GPU_TO_CPU : MemoryType::CPU_ONLY;
            desc.usage  = BufferUsageFlagBit::TRANSFER_DST;
            BufferPtr buffer(device->CreateBuffer(desc));
            EXPECT_TRUE(buffer);
            const auto imported = builder.Import(buffer, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false);
            EXPECT_TRUE(std::holds_alternative<GraphBufferHandle>(imported));
            if (const auto *handle = std::get_if<GraphBufferHandle>(&imported)) {
                if (const auto *from = std::get_if<GraphBufferHandle>(&source)) {
                    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({*from, *handle, 0, 0, 64})));
                } else {
                    const auto *imageFrom = std::get_if<GraphImageHandle>(&source);
                    EXPECT_TRUE(imageFrom);
                    if (!imageFrom)
                        return {};
                    uint64_t offset = 0;
                    for (uint32_t layer = 0; layer < layout.arrayLayers; ++layer) {
                        for (uint32_t mip = 0; mip < layout.mipLevels; ++mip) {
                            BufferImageCopyPass copy;
                            copy.buffer              = *handle;
                            copy.image               = *imageFrom;
                            copy.direction           = ImageCopyDirection::IMAGE_TO_BUFFER;
                            copy.region.bufferOffset = offset;
                            copy.region.subRange     = {mip, layer, 1, AspectFlagBit::COLOR_BIT};
                            copy.region.imageExtent  = {std::max(1u, layout.width >> mip), std::max(1u, layout.height >> mip), 1};
                            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy(copy)));
                            offset += uint64_t(copy.region.imageExtent.width) * copy.region.imageExtent.height * RenderImageTexelBytes(layout.format);
                        }
                    }
                }
                if (exported)
                    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({*handle, 0, desc.size})));
            }
            return buffer;
        }
        void ExpectBytes(Buffer &buffer, uint8_t value)
        {
            const auto *mapped = buffer.Map();
            ASSERT_TRUE(mapped);
            for (uint32_t i = 0; i < 64; ++i)
                EXPECT_EQ(mapped[i], value);
            buffer.UnMap();
        }
        void ExpectImage(Buffer &buffer, const RenderImageLayout &layout, uint8_t version)
        {
            const auto  expected = ImageBytes(layout, version);
            const auto *mapped   = buffer.Map();
            ASSERT_TRUE(mapped);
            EXPECT_TRUE(std::equal(expected.begin(), expected.end(), mapped));
            buffer.UnMap();
        }
        void Complete(size_t expected = 1)
        {
            device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
            std::array<FrameCompletion, 4> completed;
            const auto                     count = uploader->PollCompleted(completed);
            ASSERT_EQ(count, expected);
            for (size_t i = 0; i < count; ++i)
                EXPECT_EQ(completed[i].status, FrameCompletionStatus::COMPLETE);
        }
        Device                             *device = nullptr;
        DeviceFrameContextInitInfo          info;
        std::unique_ptr<DeviceFrameContext> context;
        std::unique_ptr<ResourceCatalog>    catalog;
        std::unique_ptr<ResourceUploader>   uploader;
        CpuSceneStore                       store;
        uint64_t                            packetSequence = 0;
        ComPtr<ID3D12InfoQueue>             debug;
    };
    class ChunkUploads : public ResourceUploads {
    protected:
        void Advance(const ResourceTicket &ticket)
        {
            const auto        lease = Begin();
            FrameGraphBuilder builder;
            const auto        batch = Prepare(lease, builder, {ticket});
            ASSERT_EQ(batch.admitted.size(), 1u);
            auto recording = Record(lease, Freeze(builder));
            ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
            Complete();
        }
    };
    class ImageUploads : public ChunkUploads {};
    class BufferUploads : public ChunkUploads {};
    class TrackedUploads : public ChunkUploads {
    protected:
        void TrackingLimit(uint32_t count)
        {
            uploader.reset();
            context.reset();
            info.trackedResources = count;
            context.reset(device->CreateFrameContext(info));
            ASSERT_TRUE(context);
            ResetUploader();
        }
        FrameResource Target(const FrozenFrameGraph &graph, size_t pass = 0)
        {
            const auto &data = graph.GetData();
            if (const auto *copy = std::get_if<BufferCopyPass>(&data.passes[pass]))
                return FrameResource{data.buffers[copy->destination.index].buffer};
            return FrameResource{data.images[std::get<BufferImageCopyPass>(data.passes[pass]).image.index].image.GetOwned()};
        }
        FrameResourceStateSnapshot Snapshot(const FrameResource &resource)
        {
            const auto  state    = context->GetResourceState(resource);
            const auto *snapshot = std::get_if<FrameResourceStateSnapshot>(&state);
            EXPECT_TRUE(snapshot);
            return snapshot ? *snapshot : FrameResourceStateSnapshot{};
        }
    };
    class SampledRaster : public ResourceUploads {
    protected:
        std::variant<FrameError, ShaderPtr> ResolveSource(const std::string &code, std::vector<RasterDynamicUniform> dynamicUniforms = {})
        {
            ShaderCompilerSlang compiler;
            auto               *previous = ShaderCompilerFactory::Get().GetCompiler();
            ShaderCompilerFactory::Get().Register(&compiler);
            Finally          restore{[&] { ShaderCompilerFactory::Get().Register(previous); }};
            ShaderFileSystem source;
            source.AddVirtualFile("Sampled.slang", code);
            ShaderResolver      resolver(nullptr, nullptr, source, 1, 1);
            ShaderVariantSchema schema;
            RasterShaderRequest request;
            request.vertex.relativePath = "Sampled.slang";
            request.vertex.entry        = "VSMain";
            request.vertex.stage        = ShaderStageFlagBit::VS;
            request.vertex.target       = static_cast<uint32_t>(GetParam() % 3 == 2 ? ShaderTarget::DXIL : ShaderTarget::SPIRV);
            request.vertex.schema       = &schema;
            request.vertex.schemaFp     = HashSchema(schema);
            request.fragment            = request.vertex;
            request.fragment.entry      = "FSMain";
            request.fragment.stage      = ShaderStageFlagBit::FS;
            request.dynamicUniforms     = std::move(dynamicUniforms);
            return ResolveRasterShader(*device, resolver, request);
        }
        ShaderPtr ShaderOf(uint32_t set = 1, bool arrayed = false)
        {
            std::string code = R"(
                #ifndef AURORA_DECLARATION_ATTRIBUTES
                #define AURORA_DECLARATION_ATTRIBUTES
                [__AttributeUsage(_AttributeTargets.Var)] struct AuroraResourceAttribute { int logicalSet; int logicalBinding; string owner; };
                [__AttributeUsage(_AttributeTargets.Var)] struct AuroraFieldAttribute { string path; };
                #endif
                struct ViewParams { [AuroraField("uvLod")] float4 uvLod; };
                struct MaterialParams { [AuroraField("tint")] float4 tint; };
                [AuroraResource(0, 0, "test/Sampled")] [[vk::binding(0, 0)]] ConstantBuffer<ViewParams> View : register(b0, space0);
                [AuroraResource(__SET__, 0, "test/Sampled")] [[vk::binding(0, __SET__)]] ConstantBuffer<MaterialParams> Material : register(b0, space1);
                [AuroraResource(__SET__, 1, "test/Sampled")] [[vk::binding(1, __SET__)]] Texture2D<float4> image : register(t1, space1);
                [AuroraResource(__SET__, 2, "test/Sampled")] [[vk::binding(2, __SET__)]] SamplerState imageSampler : register(s2, space1);
                struct Varying { float4 position : SV_Position; float4 sampled : COLOR; };
                Varying VSMain(uint id : SV_VertexID) {
                    Varying v;
                    v.position = float4(float2((id << 1) & 2, id & 2) * 2 - 1, 0, 1);
                    v.sampled = image.SampleLevel(imageSampler, View.uvLod.xy, View.uvLod.z);
                    return v;
                }
                float4 FSMain(Varying v) : SV_Target {
                    return (v.sampled + image.SampleLevel(imageSampler, View.uvLod.xy, View.uvLod.z)) * 0.5 * Material.tint;
                }
            )";
            for (size_t pos = 0; (pos = code.find("__SET__", pos)) != std::string::npos; pos += std::to_string(set).size())
                code.replace(pos, 7, std::to_string(set));
            if (set == 2)
                for (size_t pos = 0; (pos = code.find("space1", pos)) != std::string::npos; pos += 6)
                    code.replace(pos, 6, "space2");
            if (arrayed) {
                code.replace(code.find("Texture2D<float4>"), 17, "Texture2DArray<float4>");
                const std::string coordinate = "float3(View.uvLod.xy, View.uvLod.w)";
                for (size_t pos = 0; (pos = code.find("View.uvLod.xy", pos)) != std::string::npos; pos += coordinate.size())
                    code.replace(pos, 13, coordinate);
            }
            auto result = ResolveSource(code, {{0, 0}});
            if (const auto *error = std::get_if<FrameError>(&result)) {
                ADD_FAILURE() << error->message;
                return {};
            }
            return std::get<ShaderPtr>(result);
        }
        SamplerPtr SamplerOf(WrapMode wrap = WrapMode::REPEAT)
        {
            Sampler::Descriptor desc;
            desc.magFilter = desc.minFilter = Filter::NEAREST;
            desc.mipmapMode                 = MipFilter::NEAREST;
            desc.addressModeU = desc.addressModeV = wrap;
            return SamplerPtr(device->CreateSampler(desc));
        }
        RasterDraw DrawOf(const DeviceFrameLease &lease,
                          FrameGraphBuilder      &builder,
                          GraphImageHandle        image,
                          ShaderPtr               shader,
                          SamplerPtr              sampler,
                          uint32_t                set   = 1,
                          std::array<float, 4>    uvLod = {0.25f, 0.25f, 0.f, 0.f})
        {
            RasterDraw draw;
            RasterBindingDraft bindings;
            draw.shader   = std::move(shader);
            draw.command  = {3, 1, 0, 0};
            bindings.images   = {{set, 1, image}};
            bindings.samplers = {{set, 2, std::move(sampler)}};
            const std::array<float, 4> tint{1, 1, 1, 1};
            for (uint32_t i = 0; i < 2; ++i) {
                const auto &values    = i ? tint : uvLod;
                auto        allocated = context->WriteFrameBuffer(lease, FrameBufferKind::UNIFORM, std::as_bytes(std::span{values}));
                EXPECT_TRUE(std::holds_alternative<FrameBufferSlice>(allocated));
                if (const auto *slice = std::get_if<FrameBufferSlice>(&allocated)) {
                    auto imported = builder.Import(slice->buffer, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true);
                    EXPECT_TRUE(std::holds_alternative<GraphBufferHandle>(imported));
                    if (const auto *handle = std::get_if<GraphBufferHandle>(&imported))
                        bindings.uniforms.push_back(
                            {i ? set : 0, 0, {*handle, i ? slice->offset : 0, slice->size}, i ? 0 : static_cast<uint32_t>(slice->offset)});
                }
            }
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(SetRasterBindings(draw, std::move(bindings))));
            return draw;
        }
        GraphImageHandle Target(FrameGraphBuilder &builder)
        {
            Image::Descriptor desc;
            desc.format       = PixelFormat::RGBA8_UNORM;
            desc.extent       = {4, 4, 1};
            desc.usage        = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_SRC;
            const auto result = builder.CreateTransient(desc);
            EXPECT_TRUE(std::holds_alternative<GraphImageHandle>(result));
            const auto *handle = std::get_if<GraphImageHandle>(&result);
            return handle ? *handle : GraphImageHandle{};
        }
        BufferPtr Raster(FrameGraphBuilder &builder, GraphImageHandle target, std::vector<RasterDraw> draws, bool merged = false)
        {
            FrameRasterPass pass;
            pass.color = target;
            pass.draws = std::move(draws);
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
            if (merged) {
                pass.load = LoadOp::LOAD;
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(pass))));
            }
            return Readback(builder, target);
        }
        GraphImageHandle Texture(FrameGraphBuilder &builder, uint32_t layers = 1)
        {
            Image::Descriptor desc;
            desc.format      = PixelFormat::RGBA8_UNORM;
            desc.extent      = {4, 4, 1};
            desc.mipLevels   = 3;
            desc.arrayLayers = layers;
            desc.usage       = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
            return std::get<GraphImageHandle>(builder.CreateTransient(desc));
        }
        void UploadMip(const DeviceFrameLease &lease, FrameGraphBuilder &builder, GraphImageHandle image, uint32_t mip, uint32_t layer, uint8_t value)
        {
            const auto                   width = std::max(1u, 4u >> mip);
            const std::vector<std::byte> bytes(width * width * 4, static_cast<std::byte>(value));
            auto                         allocated = context->WriteFrameBuffer(lease, FrameBufferKind::STAGING, bytes, 4);
            ASSERT_TRUE(std::holds_alternative<FrameBufferSlice>(allocated));
            const auto &slice = std::get<FrameBufferSlice>(allocated);
            auto buffer = std::get<GraphBufferHandle>(builder.Import(slice.buffer, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true));
            BufferImageCopyPass copy;
            copy.buffer              = buffer;
            copy.image               = image;
            copy.region.bufferOffset = slice.offset;
            copy.region.subRange     = {mip, layer, 1, AspectFlagBit::COLOR_BIT};
            copy.region.imageExtent  = {width, width, 1};
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy(copy)));
        }
    };
    class SampledViews : public SampledRaster {};
    class VulkanSampledViews : public SampledRaster {};
} // namespace

TEST_P(ResourceUploads, SameGraphCopyAndOnlySuccessfulCompletionPublishResident)
{
    const auto        ticket = Supply();
    const auto        lease  = Begin();
    FrameGraphBuilder builder;
    const auto        batch = Prepare(lease, builder, {ticket, ticket});
    ASSERT_EQ(batch.admitted.size(), 1u);
    EXPECT_EQ(Status(ticket).state, RenderResourceState::UPLOAD_QUEUED);
    EXPECT_EQ(Status(ticket).submission.serial, 0u);
    const auto        handle   = uploader->ResolveInGraph(batch.id, ticket, builder);
    auto              readback = Readback(builder, handle);
    FrameGraphBuilder other;
    EXPECT_TRUE(std::holds_alternative<FrameError>(uploader->ResolveInGraph(batch.id, ticket, other)));
    auto recording = Record(lease, Freeze(builder));
    ASSERT_TRUE(recording);
    EXPECT_EQ(recording->GetGraphReceipt()->passes, (std::vector<uint32_t>{0, 1}));
    EXPECT_EQ(Status(ticket).state, RenderResourceState::UPLOAD_QUEUED);
    const auto submitted = uploader->Submit(batch.id, recording);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(submitted));
    EXPECT_EQ(Status(ticket).state, RenderResourceState::UPLOAD_SUBMITTED);
    EXPECT_EQ(Status(ticket).submission, std::get<SubmissionAccepted>(submitted).token);
    EXPECT_EQ(catalog->CpuBytes(), 0u);
    EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
    EXPECT_TRUE(std::holds_alternative<FrameError>(uploader->ResolveInGraph(batch.id, ticket, builder)));
    Complete();
    EXPECT_EQ(Status(ticket).state, RenderResourceState::RESIDENT);
    EXPECT_EQ(uploader->InflightBytes(), 0u);
    const auto frozen = catalog->Freeze(store);
    ASSERT_EQ(frozen.pins.size(), 1u);
    EXPECT_EQ(std::visit([](const auto &backing) { return backing.initializedBy; }, *frozen.pins.front()->backing),
              std::get<SubmissionAccepted>(submitted).token);
    ExpectBytes(*readback, 1);
}

TEST_P(ResourceUploads, UploadOnlySurvivesCullingAndImportsCompletedStateInNextGraph)
{
    const auto        ticket = Supply();
    const auto        lease  = Begin();
    FrameGraphBuilder builder;
    const auto        batch     = Prepare(lease, builder, {ticket});
    const auto        handle    = uploader->ResolveInGraph(batch.id, ticket, builder);
    auto              dead      = Readback(builder, handle, false);
    auto              recording = Record(lease, Freeze(builder));
    ASSERT_TRUE(recording);
    EXPECT_EQ(recording->GetGraphReceipt()->passes, (std::vector<uint32_t>{0}));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    Complete();
    const auto frozen = catalog->Freeze(store);
    ASSERT_EQ(frozen.pins.size(), 1u);
    const auto       &backing = *frozen.pins.front()->backing;
    FrameGraphBuilder next;
    const auto        source    = Import(next, backing);
    auto              readback  = Readback(next, source);
    const auto        nextLease = Begin();
    recording                   = Record(nextLease, Freeze(next));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete();
    ExpectBytes(*readback, 1);
}

TEST_P(ResourceUploads, FrameAndInflightBudgetsDeferWithoutDroppingPayload)
{
    ResetUploader({64, 64, 1, 4});
    const auto        a = Supply();
    const auto        b = Supply(1, 2);
    FrameGraphBuilder first;
    const auto        lease = Begin();
    const auto        batch = Prepare(lease, first, {b, a});
    ASSERT_EQ(batch.admitted, (std::vector<ResourceTicket>{a}));
    ASSERT_EQ(batch.deferred.size(), 1u);
    EXPECT_EQ(batch.deferred.front().reason, UploadDeferReason::FRAME_BYTES);
    EXPECT_EQ(Status(b).state, RenderResourceState::CPU_READY);
    auto recording = Record(lease, Freeze(first));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    const auto        nextLease = Begin();
    FrameGraphBuilder next;
    const auto        deferred = Prepare(nextLease, next, {b});
    EXPECT_FALSE(deferred.id);
    ASSERT_EQ(deferred.deferred.size(), 1u);
    EXPECT_EQ(deferred.deferred.front().reason, UploadDeferReason::INFLIGHT);
    EXPECT_EQ(catalog->BorrowPayload(b)->bytes.size(), 64u);
    Complete();
    const auto admitted = Prepare(nextLease, next, {b});
    ASSERT_EQ(admitted.admitted.size(), 1u);
    recording = Record(nextLease, Freeze(next));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(admitted.id, recording)));
    Complete();
    EXPECT_EQ(Status(b).state, RenderResourceState::RESIDENT);
}

TEST_P(ResourceUploads, RejectedRecordingPreservesCpuDataAndOriginalRecordingCanRetry)
{
    const auto        ticket = Supply();
    const auto        lease  = Begin();
    FrameGraphBuilder builder;
    const auto        batch     = Prepare(lease, builder, {ticket});
    auto              recording = Record(lease, Freeze(builder));
    ASSERT_TRUE(recording);
    std::unique_ptr<RecordedFrame> foreign = std::make_unique<UnissuedRecording>(*recording);
    EXPECT_TRUE(std::holds_alternative<SubmissionRejected>(uploader->Submit(batch.id, foreign)));
    EXPECT_TRUE(foreign);
    EXPECT_EQ(Status(ticket).state, RenderResourceState::UPLOAD_QUEUED);
    EXPECT_EQ(Status(ticket).submission.serial, 0u);
    EXPECT_TRUE(catalog->BorrowPayload(ticket));
    foreign.reset();
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    Complete();
    EXPECT_EQ(Status(ticket).state, RenderResourceState::RESIDENT);
}

TEST_P(ResourceUploads, CreationAndStagingLimitsKeepDeferredPayloadRetryable)
{
    const auto a = Supply();
    const auto b = Supply(1, 2);
    for (bool stagingLimit : {false, true}) {
        uploader.reset();
        context.reset();
        info.allocations.stagingBytes = stagingLimit ? 64 : 256;
        context.reset(device->CreateFrameContext(info));
        ASSERT_TRUE(context);
        ResetUploader({256, 512, stagingLimit ? 8u : 1u, 4});
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch = Prepare(lease, builder, {a, b});
        ASSERT_EQ(batch.admitted.size(), 1u);
        ASSERT_EQ(batch.deferred.size(), 1u);
        EXPECT_EQ(batch.deferred.front().reason, stagingLimit ? UploadDeferReason::STAGING : UploadDeferReason::CREATES);
        EXPECT_EQ(Status(b).state, RenderResourceState::CPU_READY);
        EXPECT_EQ(catalog->CpuBytes(), 128u);
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(uploader->Discard(batch.id)));
        EXPECT_EQ(Status(a).state, RenderResourceState::CPU_READY);
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    }
}

TEST_P(ResourceUploads, MissingCopyReceiptAndMutatedVersionCannotPublish)
{
    for (bool overwrite : {false, true}) {
        const auto        ticket = Supply(0, 1, 1, overwrite ? 2 : 1, overwrite);
        const auto        lease  = Begin();
        FrameGraphBuilder builder;
        const auto        batch    = Prepare(lease, builder, {ticket});
        const auto        original = Freeze(builder);
        auto              data     = original->GetData();
        if (!overwrite) {
            std::visit([](auto &copy) { copy.sideEffect = false; }, data.passes.front());
            auto surviving = data.passes.front();
            std::visit([](auto &copy) { copy.sideEffect = true; }, surviving);
            data.passes.push_back(surviving);
        } else {
            data.passes.push_back(data.passes.front());
        }
        auto modified  = std::make_shared<const FrozenFrameGraph>(original->GetId(), std::move(data));
        auto recording = Record(lease, std::move(modified));
        ASSERT_TRUE(recording);
        if (!overwrite)
            EXPECT_EQ(recording->GetGraphReceipt()->passes, (std::vector<uint32_t>{1}));
        EXPECT_TRUE(std::holds_alternative<SubmissionRejected>(uploader->Submit(batch.id, recording)));
        EXPECT_EQ(Status(ticket).state, RenderResourceState::UPLOAD_QUEUED);
        recording.reset();
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(uploader->Discard(batch.id)));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        EXPECT_EQ(Status(ticket).state, RenderResourceState::CPU_READY);
        // The second iteration reuses the retained payload after the rejected candidate.
        if (!overwrite) {
            ASSERT_FALSE(catalog->Fail(ticket, {RendererErrorCode::NOT_READY, "test rebuild"}));
        }
    }
}

TEST_P(ResourceUploads, CompileAndEncodeFailureRollbackAfterDrain)
{
    const auto ticket = Supply();
    {
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch = Prepare(lease, builder, {ticket});
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy(BufferCopyPass{})));
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->Compile(lease, Freeze(builder))));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(uploader->Discard(batch.id)));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        EXPECT_EQ(Status(ticket).state, RenderResourceState::CPU_READY);
    }
    const auto        lease = Begin();
    FrameGraphBuilder builder;
    const auto        batch    = Prepare(lease, builder, {ticket});
    auto              compiled = context->Compile(lease, Freeze(builder));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto         plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    UploadFaults faults(*device);
    faults.FailEncoding();
    const auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = Collect(std::get<EncodingTicket>(dispatched));
    EXPECT_TRUE(std::holds_alternative<EncodingFailed>(collected));
    faults.Restore();
    plan.reset();
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(uploader->Discard(batch.id)));
    EXPECT_EQ(Status(ticket).state, RenderResourceState::CPU_READY);
    EXPECT_EQ(uploader->InflightBytes(), 0u);
    EXPECT_TRUE(catalog->BorrowPayload(ticket));
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(ResourceUploads, CancelledSubmittedVersionCannotReplaceNewAttempt)
{
    const auto                old   = Supply();
    const auto                lease = Begin();
    FrameGraphBuilder         builder;
    const auto                batch     = Prepare(lease, builder, {old});
    auto                      graph     = Freeze(builder);
    std::weak_ptr<const void> weak      = TrackGraphGpuPayload(graph);
    auto                      recording = Record(lease, std::move(graph));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    ASSERT_FALSE(catalog->Cancel(Subscriber()));
    EXPECT_EQ(Status(old).state, RenderResourceState::CANCELLED);
    const auto fresh = Supply(0, 1, 1, 2, true);
    EXPECT_NE(fresh, old);
    EXPECT_FALSE(weak.expired());
    Complete();
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(Status(fresh).state, RenderResourceState::CPU_READY);
    EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
}

TEST_P(ResourceUploads, TwoInflightVersionsPreserveFrozenMetadataAndOldGpuBytes)
{
    const auto first = Supply();
    {
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch     = Prepare(lease, builder, {first});
        auto              recording = Record(lease, Freeze(builder));
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
        Complete();
    }
    auto old = catalog->Freeze(store);
    ASSERT_EQ(old.pins.size(), 1u);
    std::weak_ptr<const ResourceBacking> weak = old.pins.front()->backing;
    Apply({UpdateRenderEntity{Subscriber().entity, Desc(3, 2), 1}});
    const auto latest = Supply(0, 3, 2, 2);
    const auto second = Supply(1, 2);
    for (const auto &ticket : {latest, second}) {
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch     = Prepare(lease, builder, {ticket});
        auto              recording = Record(lease, Freeze(builder));
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    }
    EXPECT_EQ(uploader->InflightBytes(), 128u);
    EXPECT_EQ(catalog->Freeze(store).cpu->front().selected->resource, Key());
    Complete(2);
    EXPECT_EQ(catalog->Freeze(store).cpu->front().selected->resource, Key(3));
    EXPECT_FLOAT_EQ(old.cpu->front().selected->metadata.bounds->max.x, 1);
    EXPECT_EQ(catalog->Prune(store), 1u);
    EXPECT_FALSE(weak.expired());
    {
        const auto       &backing = *old.pins.front()->backing;
        FrameGraphBuilder builder;
        const auto        source    = Import(builder, backing);
        auto              readback  = Readback(builder, source);
        auto              recording = Record(Begin(), Freeze(builder));
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
        Complete();
        ExpectBytes(*readback, 1);
    }
    old = {};
    EXPECT_TRUE(weak.expired());
}

TEST_P(ResourceUploads, EncodingCancellationDrainsBeforeBatchRollback)
{
    const auto        ticket = Supply();
    const auto        lease  = Begin();
    FrameGraphBuilder builder;
    const auto        batch    = Prepare(lease, builder, {ticket});
    auto              compiled = context->Compile(lease, Freeze(builder));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto       plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    const auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    const auto encoding = std::get<EncodingTicket>(dispatched);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->Cancel(encoding)));
    EXPECT_TRUE(std::holds_alternative<EncodingCancelled>(Collect(encoding)));
    EXPECT_EQ(Status(ticket).state, RenderResourceState::UPLOAD_QUEUED);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(uploader->Discard(batch.id)));
    EXPECT_EQ(Status(ticket).state, RenderResourceState::CPU_READY);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(ResourceUploads, UncertainSubmissionAndFailedCompletionNeverPublishOrReuse)
{
    const auto                ticket = Supply();
    const auto                lease  = Begin();
    FrameGraphBuilder         builder;
    const auto                batch     = Prepare(lease, builder, {ticket});
    auto                      graph     = Freeze(builder);
    std::weak_ptr<const void> weak      = TrackGraphGpuPayload(graph);
    auto                      recording = Record(lease, std::move(graph));
    UploadFaults              faults(*device);
    faults.UncertainSubmit();
    EXPECT_TRUE(std::holds_alternative<SubmissionIndeterminate>(uploader->Submit(batch.id, recording)));
    EXPECT_FALSE(recording);
    EXPECT_EQ(Status(ticket).state, RenderResourceState::FAILED);
    EXPECT_EQ(Status(ticket).submission.serial, 0u);
    EXPECT_EQ(uploader->InflightBytes(), 64u);
    EXPECT_FALSE(weak.expired());
    faults.Restore();
    faults.FailQuery();
    std::array<FrameCompletion, 1> completed;
    ASSERT_EQ(uploader->PollCompleted(completed), 1u);
    EXPECT_EQ(completed.front().status, FrameCompletionStatus::DEVICE_LOST);
    EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
    FrameGraphBuilder next;
    EXPECT_TRUE(std::holds_alternative<FrameError>(uploader->Prepare(lease, next, std::span<const ResourceTicket>(&ticket, 1))));
    EXPECT_FALSE(weak.expired());
    faults.Restore();
    uploader.reset();
    context.reset();
    EXPECT_TRUE(weak.expired());
}

TEST_P(ResourceUploads, AcceptedUploadQueryFailureDoesNotBecomeResident)
{
    const auto        ticket = Supply();
    const auto        lease  = Begin();
    FrameGraphBuilder builder;
    const auto        batch     = Prepare(lease, builder, {ticket});
    auto              recording = Record(lease, Freeze(builder));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    UploadFaults faults(*device);
    faults.FailQuery();
    std::array<FrameCompletion, 1> completed;
    ASSERT_EQ(uploader->PollCompleted(completed), 1u);
    EXPECT_EQ(Status(ticket).state, RenderResourceState::FAILED);
    EXPECT_EQ(Status(ticket).error.code, RendererErrorCode::DEVICE_LOST);
    EXPECT_NE(Status(ticket).submission.serial, 0u);
    EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
    EXPECT_EQ(uploader->InflightBytes(), 64u);
    faults.Restore();
}

TEST_P(ImageUploads, CompleteMipLayersConsumeInGraphAndPublishAfterPoll)
{
    uploader.reset();
    context.reset();
    info.allocations.stagingBytes = 2048;
    context.reset(device->CreateFrameContext(info));
    ASSERT_TRUE(context);
    ResetUploader();
    const RenderImageLayout         layout{9, 5, 4, 2};
    const auto                      ticket = Supply(0, 1, 1, 1, false, layout);
    const auto                      lease  = Begin();
    const std::array<std::byte, 20> prefix{};
    ASSERT_TRUE(std::holds_alternative<FrameBufferSlice>(context->WriteFrameBuffer(lease, FrameBufferKind::STAGING, prefix, 4)));
    FrameGraphBuilder builder;
    const auto        batch = Prepare(lease, builder, {ticket});
    ASSERT_EQ(batch.admitted.size(), 1u);
    EXPECT_EQ(builder.GetPassCount(), 8u);
    const auto handle = uploader->ResolveInGraph(batch.id, ticket, builder);
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(handle));
    auto readback = Readback(builder, handle, true, layout);
    auto graph    = Freeze(builder);
    EXPECT_GE(std::get<BufferImageCopyPass>(graph->GetData().passes.front()).region.bufferOffset, 20u);
    auto recording = Record(lease, std::move(graph));
    ASSERT_TRUE(recording);
    EXPECT_EQ(recording->GetGraphReceipt()->passes.size(), 16u);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
    EXPECT_EQ(Status(ticket).state, RenderResourceState::UPLOAD_SUBMITTED);
    EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
    Complete();
    EXPECT_EQ(Status(ticket).state, RenderResourceState::RESIDENT);
    ExpectImage(*readback, layout, 1);
    const auto frozen = catalog->Freeze(store);
    ASSERT_EQ(frozen.pins.size(), 1u);
    const auto &backing = std::get<ResourceImageBacking>(*frozen.pins.front()->backing);
    EXPECT_EQ(backing.state, ImageLayout::GENERAL);
    EXPECT_EQ(backing.descriptor.mipLevels, 4u);
    EXPECT_EQ(frozen.pins.front()->cpu->metadata.image->arrayLayers, 2u);
    FrameGraphBuilder next;
    readback  = Readback(next, Import(next, *frozen.pins.front()->backing), true, layout);
    recording = Record(Begin(), Freeze(next));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete();
    ExpectImage(*readback, layout, 1);
}

TEST_P(ImageUploads, MissingMipMutatedRegionAndOverwriteCannotPublish)
{
    const RenderImageLayout layout{4, 4, 2, 2};
    const auto              ticket = Supply(0, 1, 1, 1, false, layout);
    for (uint32_t fault = 0; fault < 7; ++fault) {
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch    = Prepare(lease, builder, {ticket});
        const auto        original = Freeze(builder);
        auto              data     = original->GetData();
        ASSERT_EQ(data.passes.size(), 4u);
        if (fault == 0)
            std::get<BufferImageCopyPass>(data.passes.back()).sideEffect = false;
        else if (fault == 1)
            data.passes.push_back(data.passes.front());
        else if (fault == 2)
            std::get<BufferImageCopyPass>(data.passes.front()).region.subRange.baseLayer = 1;
        else if (fault == 3)
            data.images.front().final = ImageLayout::TRANSFER_SRC;
        else if (fault == 4)
            std::get<BufferImageCopyPass>(data.passes.front()).region.imageExtent.width = 2;
        else if (fault == 5)
            std::get<BufferImageCopyPass>(data.passes.front()).region.bufferRowLength = 4;
        else {
            data.images.front().initial            = ImageLayout::GENERAL;
            data.images.front().initializedRegions = {{0, 0, {{0, 0}, {1, 1}}}};
        }
        auto                           modified = std::make_shared<const FrozenFrameGraph>(original->GetId(), std::move(data));
        std::unique_ptr<RecordedFrame> recording;
        if (fault == 6) {
            const auto compiled = context->Compile(lease, modified);
            ASSERT_TRUE(std::holds_alternative<FrameError>(compiled));
            EXPECT_EQ(std::get<FrameError>(compiled).code, FrameErrorCode::INVALID_STATE);
        } else {
            recording = Record(lease, modified);
            ASSERT_TRUE(recording);
            EXPECT_TRUE(std::holds_alternative<SubmissionRejected>(uploader->Submit(batch.id, recording)));
        }
        EXPECT_EQ(Status(ticket).state, RenderResourceState::UPLOAD_QUEUED);
        EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
        recording.reset();
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(uploader->Discard(batch.id)));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        EXPECT_EQ(Status(ticket).state, RenderResourceState::CPU_READY);
        EXPECT_EQ(catalog->CpuBytes(), 160u);
    }
}

TEST_P(ImageUploads, MixedBatchBudgetsAdmitChunksAndKeepDeferredData)
{
    uploader.reset();
    context.reset();
    info.allocations.stagingBytes = 1024;
    context.reset(device->CreateFrameContext(info));
    ASSERT_TRUE(context);
    const RenderImageLayout layout{9, 5, 4, 2};
    const auto              image   = Supply(0, 1, 1, 1, false, layout);
    const auto              request = std::get<ResourceRequest>(catalog->Request(store, Subscriber(1)));
    RenderResourcePayload   payload{Key(2), Name("buffer-v1"), std::vector<uint8_t>(64, 2), {}};
    ASSERT_FALSE(catalog->Supply(request.ticket, payload));
    for (bool inflight : {false, true}) {
        ResetUploader({inflight ? 512u : 447u, inflight ? 447u : 512u, 2, 4});
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch = Prepare(lease, builder, {image, request.ticket});
        EXPECT_EQ(batch.admitted, (std::vector<ResourceTicket>{image}));
        ASSERT_EQ(batch.deferred.size(), 1u);
        EXPECT_EQ(batch.deferred.front().reason, inflight ? UploadDeferReason::INFLIGHT : UploadDeferReason::FRAME_BYTES);
        EXPECT_EQ(builder.GetPassCount(), 7u);
        EXPECT_EQ(uploader->InflightBytes(), 444u);
        EXPECT_EQ(Status(image).state, RenderResourceState::UPLOAD_QUEUED);
        EXPECT_EQ(catalog->BorrowPayload(image)->bytes.size(), 448u);
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(uploader->Discard(batch.id)));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    }
    ResetUploader({512, 512, 2, 4});
    const auto        lease = Begin();
    FrameGraphBuilder builder;
    const auto        batch = Prepare(lease, builder, {image, request.ticket});
    EXPECT_EQ(batch.admitted.size(), 2u);
    EXPECT_EQ(uploader->InflightBytes(), 512u);
    auto texture   = Readback(builder, uploader->ResolveInGraph(batch.id, image, builder), true, layout);
    auto buffer    = Readback(builder, uploader->ResolveInGraph(batch.id, request.ticket, builder));
    auto recording = Record(lease, Freeze(builder));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    Complete();
    EXPECT_EQ(Status(image).state, RenderResourceState::RESIDENT);
    EXPECT_EQ(Status(request.ticket).state, RenderResourceState::RESIDENT);
    ExpectImage(*texture, layout, 1);
    ExpectBytes(*buffer, 2);
}

TEST_P(ImageUploads, AdmissionFailureRollsBackEarlierItemsAndCanRetryWithFreshLease)
{
    const auto            request = std::get<ResourceRequest>(catalog->Request(store, Subscriber()));
    RenderResourcePayload payload{Key(), Name("buffer-v1"), std::vector<uint8_t>(64, 1), {}};
    ASSERT_FALSE(catalog->Supply(request.ticket, payload));
    const auto        image = Supply(1, 2);
    const auto        lease = Begin();
    FrameGraphBuilder full;
    for (uint32_t i = 0; i < 1023; ++i)
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(full.Copy(BufferCopyPass{})));
    const std::array tickets{request.ticket, image};
    EXPECT_TRUE(std::holds_alternative<FrameError>(uploader->Prepare(lease, full, tickets)));
    EXPECT_EQ(Status(request.ticket).state, RenderResourceState::CPU_READY);
    EXPECT_EQ(Status(image).state, RenderResourceState::CPU_READY);
    EXPECT_EQ(catalog->CpuBytes(), 128u);
    EXPECT_EQ(uploader->InflightBytes(), 0u);
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    const auto        nextLease = Begin();
    FrameGraphBuilder next;
    const auto        batch = Prepare(nextLease, next, {request.ticket, image});
    ASSERT_EQ(batch.admitted.size(), 2u);
    auto recording = Record(nextLease, Freeze(next));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    Complete();
    EXPECT_EQ(Status(request.ticket).state, RenderResourceState::RESIDENT);
    EXPECT_EQ(Status(image).state, RenderResourceState::RESIDENT);
}

TEST_P(ImageUploads, UnsupportedSubresourceCountFailsBeforeStagingOrQueueing)
{
    const auto        ticket = Supply(0, 1, 1, 1, false, RenderImageLayout{1, 1, 1, 1025});
    const auto        lease  = Begin();
    FrameGraphBuilder builder;
    const auto        result = uploader->Prepare(lease, builder, std::span{&ticket, 1});
    ASSERT_TRUE(std::holds_alternative<FrameError>(result));
    EXPECT_EQ(std::get<FrameError>(result).code, FrameErrorCode::UNSUPPORTED);
    EXPECT_EQ(builder.GetPassCount(), 0u);
    EXPECT_EQ(Status(ticket).state, RenderResourceState::CPU_READY);
    EXPECT_EQ(catalog->CpuBytes(), 4100u);
    EXPECT_EQ(uploader->InflightBytes(), 0u);
    const std::array<std::byte, 256> bytes{};
    EXPECT_TRUE(std::holds_alternative<FrameBufferSlice>(context->WriteFrameBuffer(lease, FrameBufferKind::STAGING, bytes, 4)));
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(ImageUploads, ChunksCrossRowsMipsAndLayersAndPublishOnlyCompleteBytes)
{
    const RenderImageLayout layout{9, 5, 4, 2};
    const auto              request = std::get<ResourceRequest>(catalog->Request(store, Subscriber()));
    RenderResourcePayload   payload{Key(), Name("image-v1"), std::vector<uint8_t>(*RenderImageByteSize(layout)), {}};
    payload.metadata.image = layout;
    for (size_t i = 0; i < payload.bytes.size(); ++i)
        payload.bytes[i] = static_cast<uint8_t>(i * 17 + i / 37);
    const auto expected = payload.bytes;
    ASSERT_FALSE(catalog->Supply(request.ticket, payload));
    ResetUploader({20, 40, 1, 4});
    ImagePtr  backing;
    BufferPtr readback;
    for (uint64_t completed = 0; completed < expected.size();) {
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch = Prepare(lease, builder, {request.ticket});
        ASSERT_EQ(batch.admitted.size(), 1u);
        const auto bytes = std::min<uint64_t>(20, expected.size() - completed);
        const bool final = completed + bytes == expected.size();
        EXPECT_EQ(Status(request.ticket).upload.submittedBytes, completed);
        EXPECT_EQ(uploader->PendingResourceBytes(), expected.size());
        auto resolved = uploader->ResolveInGraph(batch.id, request.ticket, builder);
        if (final)
            readback = Readback(builder, resolved, true, layout);
        else
            EXPECT_TRUE(std::holds_alternative<FrameError>(resolved));
        auto graph = Freeze(builder);
        ASSERT_TRUE(graph);
        const auto &image = graph->GetData().images.front();
        if (!backing)
            backing = image.image.GetOwned();
        EXPECT_EQ(image.image.Get(), backing.Get());
        const auto contents = ResolveInitializedImageContents(image);
        ASSERT_TRUE(std::holds_alternative<ResolvedImageContents>(contents));
        const auto prefix = completed ? std::vector<ImageContentSpan>{{0, completed / 4}} : std::vector<ImageContentSpan>{};
        EXPECT_EQ(std::get<ResolvedImageContents>(contents).ranges, prefix);
        auto recording = Record(lease, graph);
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
        EXPECT_EQ(Status(request.ticket).upload.completedBytes, completed);
        EXPECT_EQ(Status(request.ticket).upload.submittedBytes, completed + bytes);
        EXPECT_EQ(catalog->CpuBytes(), final ? 0u : expected.size());
        EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
        ResourceImageBacking premature{backing, image.descriptor, ImageLayout::GENERAL, Status(request.ticket).submission};
        EXPECT_TRUE(catalog->AdoptResident(request.ticket, request.ticket.resource, std::make_shared<const ResourceBacking>(premature)));
        if (!completed) {
            const auto        other = Begin();
            FrameGraphBuilder blocked;
            const auto        deferred = Prepare(other, blocked, {request.ticket});
            ASSERT_EQ(deferred.deferred.size(), 1u);
            EXPECT_EQ(deferred.deferred.front().reason, UploadDeferReason::NOT_READY);
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(other)));
        }
        Complete();
        completed += bytes;
        EXPECT_EQ(Status(request.ticket).upload.completedBytes, completed);
        EXPECT_EQ(Status(request.ticket).state, final ? RenderResourceState::RESIDENT : RenderResourceState::CPU_READY);
        EXPECT_EQ(uploader->InflightBytes(), 0u);
    }
    ASSERT_TRUE(readback);
    const auto *mapped = readback->Map();
    ASSERT_TRUE(mapped);
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), mapped));
    readback->UnMap();
    EXPECT_EQ(uploader->PendingResourceBytes(), 0u);
    const auto frozen = catalog->Freeze(store);
    ASSERT_EQ(frozen.pins.size(), 1u);
    EXPECT_EQ(std::get<ResourceImageBacking>(*frozen.pins.front()->backing).image.Get(), backing.Get());
}

TEST_P(ImageUploads, HalfFloatChunksPreserveMipLayerBytesAndTexelPrefixes)
{
    const RenderImageLayout layout{7, 3, 3, 2, RenderImageFormat::RGBA16_FLOAT};
    const auto              request = std::get<ResourceRequest>(catalog->Request(store, Subscriber()));
    RenderResourcePayload   payload{Key(), Name("image-half"), std::vector<uint8_t>(*RenderImageByteSize(layout)), {}};
    payload.metadata.image = layout;
    for (size_t i = 0; i < payload.bytes.size(); ++i)
        payload.bytes[i] = static_cast<uint8_t>(i * 13 + i / 29);
    const auto expected = payload.bytes;
    ASSERT_FALSE(catalog->Supply(request.ticket, payload));
    ResetUploader({28, 56, 1, 4});
    BufferPtr readback;
    for (uint64_t completed = 0; completed < expected.size();) {
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch = Prepare(lease, builder, {request.ticket});
        ASSERT_EQ(batch.admitted.size(), 1u);
        const auto bytes    = std::min<uint64_t>(24, expected.size() - completed);
        const bool final    = completed + bytes == expected.size();
        auto       resolved = uploader->ResolveInGraph(batch.id, request.ticket, builder);
        if (final)
            readback = Readback(builder, resolved, true, layout);
        else
            EXPECT_TRUE(std::holds_alternative<FrameError>(resolved));
        auto graph = Freeze(builder);
        ASSERT_TRUE(graph);
        const auto contents = ResolveInitializedImageContents(graph->GetData().images.front());
        ASSERT_TRUE(std::holds_alternative<ResolvedImageContents>(contents));
        const auto prefix = completed ? std::vector<ImageContentSpan>{{0, completed / 8}} : std::vector<ImageContentSpan>{};
        EXPECT_EQ(std::get<ResolvedImageContents>(contents).ranges, prefix);
        auto recording = Record(lease, graph);
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
        Complete();
        completed += bytes;
        EXPECT_EQ(Status(request.ticket).upload.completedBytes, completed);
        EXPECT_EQ(Status(request.ticket).state, final ? RenderResourceState::RESIDENT : RenderResourceState::CPU_READY);
    }
    ASSERT_TRUE(readback);
    const auto *mapped = readback->Map();
    ASSERT_TRUE(mapped);
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), mapped));
    readback->UnMap();
}

TEST_P(ChunkUploads, ChunkSizeUsesAlignedRemainingStagingSpace)
{
    uploader.reset();
    context.reset();
    info.allocations.stagingBytes = 27;
    context.reset(device->CreateFrameContext(info));
    ASSERT_TRUE(context);
    ResetUploader({128, 128, 1, 4});
    const auto ticket = Supply();
    for (uint64_t completed = 0; completed < 64;) {
        const auto                     lease = Begin();
        const std::array<std::byte, 3> padding{};
        ASSERT_TRUE(std::holds_alternative<FrameBufferSlice>(context->WriteFrameBuffer(lease, FrameBufferKind::STAGING, padding)));
        FrameGraphBuilder builder;
        const auto        batch = Prepare(lease, builder, {ticket});
        ASSERT_EQ(batch.admitted.size(), 1u);
        const auto bytes = std::min<uint64_t>(20, 64 - completed);
        EXPECT_EQ(uploader->InflightBytes(), bytes);
        BufferPtr readback;
        if (completed + bytes == 64)
            readback = Readback(builder, uploader->ResolveInGraph(batch.id, ticket, builder));
        auto recording = Record(lease, Freeze(builder));
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
        Complete();
        completed += bytes;
        EXPECT_EQ(Status(ticket).upload.completedBytes, completed);
        if (readback) {
            if (GetParam() >= 3)
                ExpectImage(*readback, {4, 4}, 1);
            else
                ExpectBytes(*readback, 1);
        }
    }
}

TEST_P(ImageUploads, ContinuationRejectsChangedPrefixAndRollsBackAfterEncodingFailure)
{
    ResetUploader({20, 64, 1, 4});
    const auto ticket = Supply();
    Advance(ticket);
    ASSERT_EQ(Status(ticket).upload.completedBytes, 20u);
    ImagePtr backing;
    for (uint32_t fault = 0; fault < 4; ++fault) {
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch = Prepare(lease, builder, {ticket});
        auto              graph = Freeze(builder);
        ASSERT_TRUE(graph);
        if (!backing)
            backing = graph->GetData().images.front().image.GetOwned();
        EXPECT_EQ(graph->GetData().images.front().image.Get(), backing.Get());
        if (fault < 3) {
            auto data = graph->GetData();
            if (fault == 0)
                data.images.front().initializedRegions.clear();
            else if (fault == 1)
                data.images.front().initializedRegions.push_back({0, 0, {{3, 3}, {1, 1}}});
            auto modified  = std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data));
            auto recording = Record(lease, modified);
            ASSERT_TRUE(recording);
            if (fault == 2) {
                std::unique_ptr<RecordedFrame> foreign = std::make_unique<UnissuedRecording>(*recording);
                EXPECT_TRUE(std::holds_alternative<SubmissionRejected>(uploader->Submit(batch.id, foreign)));
                EXPECT_TRUE(foreign);
            } else {
                EXPECT_TRUE(std::holds_alternative<SubmissionRejected>(uploader->Submit(batch.id, recording)));
                EXPECT_TRUE(recording);
            }
            recording.reset();
        } else {
            auto compiled = context->Compile(lease, graph);
            ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
            auto         plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
            UploadFaults failures(*device);
            failures.FailEncoding();
            const auto dispatched = context->Dispatch(plan);
            ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
            EXPECT_TRUE(std::holds_alternative<EncodingFailed>(Collect(std::get<EncodingTicket>(dispatched))));
        }
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(uploader->Discard(batch.id)));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        EXPECT_EQ(Status(ticket).upload.submittedBytes, 20u);
        EXPECT_EQ(Status(ticket).upload.completedBytes, 20u);
        EXPECT_EQ(uploader->InflightBytes(), 0u);
        EXPECT_EQ(uploader->PendingResourceBytes(), 64u);
    }
    Advance(ticket);
    Advance(ticket);
    const auto        lease = Begin();
    FrameGraphBuilder builder;
    const auto        batch    = Prepare(lease, builder, {ticket});
    auto              readback = Readback(builder, uploader->ResolveInGraph(batch.id, ticket, builder));
    auto              graph    = Freeze(builder);
    EXPECT_EQ(graph->GetData().images.front().image.Get(), backing.Get());
    auto recording = Record(lease, graph);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    Complete();
    ExpectImage(*readback, {4, 4}, 1);
}

TEST_P(ChunkUploads, CancelledContinuationCannotAdvanceReplacementTicket)
{
    ResetUploader({20, 64, 1, 4});
    auto old = Supply();
    Advance(old);
    const auto        lease = Begin();
    FrameGraphBuilder builder;
    const auto        batch     = Prepare(lease, builder, {old});
    auto              recording = Record(lease, Freeze(builder));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    ASSERT_FALSE(catalog->Cancel(Subscriber()));
    auto fresh = Supply(0, 1, 1, 2, true);
    ASSERT_NE(fresh, old);
    EXPECT_EQ(Status(fresh).upload.completedBytes, 0u);
    Complete();
    EXPECT_EQ(Status(fresh).upload.submittedBytes, 0u);
    EXPECT_EQ(uploader->PendingResourceBytes(), 0u);
    Advance(fresh);
    EXPECT_EQ(Status(fresh).upload.completedBytes, 20u);
    ASSERT_FALSE(catalog->Cancel(Subscriber(0, 1, 2)));
    fresh = Supply(0, 1, 1, 3, true);
    Advance(fresh);
    EXPECT_EQ(Status(fresh).upload.completedBytes, 20u);
    EXPECT_EQ(uploader->PendingResourceBytes(), 64u);
}

TEST_P(ChunkUploads, PartialBackingBudgetsAndOwnerRestartRemainBounded)
{
    const auto a = Supply();
    const auto b = Supply(1, 2);
    for (bool bytes : {false, true}) {
        ResetUploader({48, 128, 1, 4, bytes ? 4u : 1u, bytes ? 100u : 128u});
        Advance(a);
        EXPECT_EQ(Status(a).upload.completedBytes, 48u);
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch = Prepare(lease, builder, {a, b});
        ASSERT_EQ(batch.admitted.size(), 1u);
        ASSERT_EQ(batch.deferred.size(), 1u);
        EXPECT_EQ(batch.deferred.front().reason, bytes ? UploadDeferReason::RESOURCE_BYTES : UploadDeferReason::RESOURCE_COUNT);
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(uploader->Discard(batch.id)));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        ResetUploader();
        EXPECT_EQ(Status(a).upload.completedBytes, 0u);
        EXPECT_EQ(Status(a).upload.submittedBytes, 0u);
        EXPECT_EQ(catalog->CpuBytes(), 128u);
    }
    ResetUploader({48, 128, 1, 4, 2, 128});
    Advance(a);
    const auto        lease = Begin();
    FrameGraphBuilder builder;
    const auto        batch = Prepare(lease, builder, {a, b});
    ASSERT_EQ(batch.admitted.size(), 2u);
    EXPECT_TRUE(batch.deferred.empty());
    auto recording = Record(lease, Freeze(builder));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    Complete();
    EXPECT_EQ(Status(a).state, RenderResourceState::RESIDENT);
    EXPECT_EQ(Status(b).upload.completedBytes, 32u);
    EXPECT_EQ(uploader->PendingResourceBytes(), 64u);
}

TEST_P(ImageUploads, CompletedLayerPrefixUsesCompactInitialRanges)
{
    uploader.reset();
    context.reset();
    info.allocations.stagingBytes = 1200;
    info.parallelNum              = 1;
    context.reset(device->CreateFrameContext(info));
    ASSERT_TRUE(context);
    ResetUploader({1196, 1200, 1, 4});
    const RenderImageLayout layout{1, 1, 1, 300};
    const auto              ticket = Supply(0, 1, 1, 1, false, layout);
    Advance(ticket);
    ASSERT_EQ(Status(ticket).upload.completedBytes, 1196u);
    const auto        lease = Begin();
    FrameGraphBuilder builder;
    const auto        batch    = Prepare(lease, builder, {ticket});
    auto              readback = Readback(builder, uploader->ResolveInGraph(batch.id, ticket, builder), true, layout);
    auto              graph    = Freeze(builder);
    ASSERT_EQ(graph->GetData().images.front().initializedRanges.size(), 1u);
    EXPECT_EQ(graph->GetData().images.front().initializedRanges.front().layers, 299u);
    auto recording = Record(lease, graph);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    Complete();
    ExpectImage(*readback, layout, 1);
}

TEST_P(ChunkUploads, FailedContinuationQuarantinesPartialBackingAndProgress)
{
    ResetUploader({20, 64, 1, 4});
    const auto ticket = Supply();
    Advance(ticket);
    const auto        lease = Begin();
    FrameGraphBuilder builder;
    const auto        batch     = Prepare(lease, builder, {ticket});
    auto              recording = Record(lease, Freeze(builder));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    UploadFaults faults(*device);
    faults.FailQuery();
    std::array<FrameCompletion, 1> completed;
    ASSERT_EQ(uploader->PollCompleted(completed), 1u);
    EXPECT_EQ(Status(ticket).state, RenderResourceState::FAILED);
    EXPECT_EQ(Status(ticket).upload.completedBytes, 20u);
    EXPECT_EQ(Status(ticket).upload.submittedBytes, 40u);
    EXPECT_EQ(uploader->InflightBytes(), 20u);
    EXPECT_EQ(uploader->PendingResourceBytes(), 64u);
    EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
    FrameGraphBuilder next;
    EXPECT_TRUE(std::holds_alternative<FrameError>(uploader->Prepare(lease, next, std::span{&ticket, 1})));
}

TEST_P(ChunkUploads, UncertainContinuationPreservesCompletedPrefixAndQuarantinesBacking)
{
    ResetUploader({20, 64, 1, 4});
    const auto ticket = Supply();
    Advance(ticket);
    const auto        lease = Begin();
    FrameGraphBuilder builder;
    const auto        batch     = Prepare(lease, builder, {ticket});
    auto              recording = Record(lease, Freeze(builder));
    UploadFaults      faults(*device);
    faults.UncertainSubmit();
    EXPECT_TRUE(std::holds_alternative<SubmissionIndeterminate>(uploader->Submit(batch.id, recording)));
    EXPECT_FALSE(recording);
    EXPECT_EQ(Status(ticket).state, RenderResourceState::FAILED);
    EXPECT_EQ(Status(ticket).upload.completedBytes, 20u);
    EXPECT_EQ(Status(ticket).upload.submittedBytes, 20u);
    EXPECT_EQ(uploader->InflightBytes(), 20u);
    EXPECT_EQ(uploader->PendingResourceBytes(), 64u);
    EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
    faults.Restore();
    faults.FailQuery();
    std::array<FrameCompletion, 1> completed;
    ASSERT_EQ(uploader->PollCompleted(completed), 1u);
    EXPECT_EQ(completed.front().status, FrameCompletionStatus::DEVICE_LOST);
    EXPECT_EQ(uploader->PendingResourceBytes(), 64u);
    faults.Restore();
    uploader.reset();
    context.reset();
}

TEST_P(ImageUploads, CopyPassBudgetSplitsContinuationAtRectangleBoundary)
{
    ResetUploader({20, 64, 1, 4});
    const auto ticket = Supply();
    Advance(ticket);
    const auto                     lease = Begin();
    const std::array<std::byte, 4> bytes{};
    auto                           staging = context->WriteFrameBuffer(lease, FrameBufferKind::STAGING, bytes);
    ASSERT_TRUE(std::holds_alternative<FrameBufferSlice>(staging));
    Buffer::Descriptor desc;
    desc.size  = 4;
    desc.usage = BufferUsageFlagBit::TRANSFER_DST;
    BufferPtr unused(device->CreateBuffer(desc));
    ASSERT_TRUE(unused);
    FrameGraphBuilder builder;
    auto source      = builder.Import(std::get<FrameBufferSlice>(staging).buffer, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true);
    auto destination = builder.Import(unused, CopyBufferState::COMMON, CopyBufferState::COMMON, false);
    ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(source));
    ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(destination));
    for (uint32_t i = 0; i < 1023; ++i)
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(
            builder.Copy({std::get<GraphBufferHandle>(source), std::get<GraphBufferHandle>(destination), 0, 0, 4})));
    const auto batch = Prepare(lease, builder, {ticket});
    ASSERT_EQ(batch.admitted.size(), 1u);
    EXPECT_EQ(uploader->InflightBytes(), 12u);
    auto        graph = Freeze(builder);
    const auto &copy  = std::get<BufferImageCopyPass>(graph->GetData().passes.back());
    EXPECT_EQ(copy.region.imageOffset.x, 1);
    EXPECT_EQ(copy.region.imageExtent.width, 3u);
    auto recording = Record(lease, graph);
    ASSERT_TRUE(recording);
    EXPECT_EQ(recording->GetGraphReceipt()->passes, (std::vector<uint32_t>{1023}));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    Complete();
    EXPECT_EQ(Status(ticket).upload.completedBytes, 32u);
}

TEST_P(BufferUploads, ChunksReuseBackingAndPreserveExactBytesAcrossGraphs)
{
    const auto            request = std::get<ResourceRequest>(catalog->Request(store, Subscriber()));
    RenderResourcePayload payload{Key(), Name("buffer-v1"), std::vector<uint8_t>(100), {}};
    for (size_t i = 0; i < payload.bytes.size(); ++i)
        payload.bytes[i] = static_cast<uint8_t>(i * 31 + 9);
    const auto expected = payload.bytes;
    ASSERT_FALSE(catalog->Supply(request.ticket, payload));
    ResetUploader({23, 40, 1, 4});
    BufferPtr backing, readback;
    for (uint64_t completed = 0; completed < expected.size(); completed += 20) {
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch = Prepare(lease, builder, {request.ticket});
        ASSERT_EQ(batch.admitted.size(), 1u);
        const bool final  = completed + 20 == expected.size();
        const auto source = uploader->ResolveInGraph(batch.id, request.ticket, builder);
        if (final) {
            Buffer::Descriptor desc;
            desc.size   = expected.size();
            desc.memory = GetParam() == 2 ? MemoryType::GPU_TO_CPU : MemoryType::CPU_ONLY;
            desc.usage  = BufferUsageFlagBit::TRANSFER_DST;
            readback    = BufferPtr(device->CreateBuffer(desc));
            ASSERT_TRUE(readback);
            const auto imported = builder.Import(readback, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false);
            ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(source));
            ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(imported));
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(
                builder.Copy({std::get<GraphBufferHandle>(source), std::get<GraphBufferHandle>(imported), 0, 0, desc.size, true})));
        } else {
            EXPECT_TRUE(std::holds_alternative<FrameError>(source));
        }
        const auto  graph  = Freeze(builder);
        const auto &copy   = std::get<BufferCopyPass>(graph->GetData().passes.front());
        const auto &target = graph->GetData().buffers[copy.destination.index];
        if (!backing)
            backing = target.buffer;
        EXPECT_EQ(target.buffer.Get(), backing.Get());
        EXPECT_EQ(target.size, 100u);
        EXPECT_EQ(copy.destinationOffset, completed);
        EXPECT_EQ(copy.size, 20u);
        const auto prefix = completed ? std::vector<BufferContentRange>{{0, completed}} : std::vector<BufferContentRange>{};
        EXPECT_EQ(std::get<std::vector<BufferContentRange>>(ResolveInitializedBufferContents(target)), prefix);
        auto recording = Record(lease, graph);
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
        EXPECT_EQ(Status(request.ticket).upload.submittedBytes, completed + 20);
        EXPECT_EQ(Status(request.ticket).upload.completedBytes, completed);
        ResourceBufferBacking premature{backing, CopyBufferState::COMMON, Status(request.ticket).submission};
        EXPECT_TRUE(catalog->AdoptResident(request.ticket, request.ticket.resource, std::make_shared<const ResourceBacking>(premature)));
        EXPECT_EQ(catalog->CpuBytes(), final ? 0u : 100u);
        EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
        Complete();
        EXPECT_EQ(Status(request.ticket).upload.completedBytes, completed + 20);
        if (!final) {
            EXPECT_TRUE(catalog->AdoptResident(request.ticket, request.ticket.resource, std::make_shared<const ResourceBacking>(premature)));
            EXPECT_TRUE(catalog->AdoptResident(request.ticket, request.ticket.resource, std::make_shared<const ResourceBacking>()));
        }
    }
    ASSERT_TRUE(readback);
    const auto *mapped = readback->Map();
    ASSERT_TRUE(mapped);
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), mapped));
    readback->UnMap();
    EXPECT_EQ(Status(request.ticket).state, RenderResourceState::RESIDENT);
    EXPECT_EQ(uploader->PendingResourceBytes(), 0u);
    EXPECT_EQ(std::get<ResourceBufferBacking>(*catalog->Freeze(store).pins.front()->backing).buffer.Get(), backing.Get());
}

TEST_P(BufferUploads, ContinuationRejectsPrefixOffsetAndOverwriteMutations)
{
    ResetUploader({20, 64, 1, 4});
    const auto ticket = Supply();
    Advance(ticket);
    ASSERT_EQ(Status(ticket).upload.completedBytes, 20u);
    BufferPtr backing;
    for (uint32_t fault = 0; fault < 5; ++fault) {
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch  = Prepare(lease, builder, {ticket});
        const auto        graph  = Freeze(builder);
        auto              data   = graph->GetData();
        auto             &copy   = std::get<BufferCopyPass>(data.passes.front());
        auto             &target = data.buffers[copy.destination.index];
        if (!backing)
            backing = target.buffer;
        EXPECT_EQ(target.buffer.Get(), backing.Get());
        if (fault == 0)
            target.initializedRanges.clear();
        else if (fault == 1)
            target.initializedRanges.push_back({60, 4});
        else if (fault == 2)
            copy.destinationOffset += 4;
        else if (fault == 3)
            copy.size -= 4;
        else
            data.passes.push_back(copy);
        auto modified  = std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data));
        auto recording = Record(lease, modified);
        ASSERT_TRUE(recording);
        EXPECT_TRUE(std::holds_alternative<SubmissionRejected>(uploader->Submit(batch.id, recording)));
        EXPECT_TRUE(recording);
        recording.reset();
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(uploader->Discard(batch.id)));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        EXPECT_EQ(Status(ticket).upload.completedBytes, 20u);
        EXPECT_EQ(Status(ticket).upload.submittedBytes, 20u);
        EXPECT_EQ(uploader->PendingResourceBytes(), 64u);
    }
    Advance(ticket);
    Advance(ticket);
    const auto        lease = Begin();
    FrameGraphBuilder builder;
    const auto        batch     = Prepare(lease, builder, {ticket});
    auto              readback  = Readback(builder, uploader->ResolveInGraph(batch.id, ticket, builder));
    auto              recording = Record(lease, Freeze(builder));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    Complete();
    ExpectBytes(*readback, 1);
}

TEST_P(BufferUploads, MixedChunksShareOneBackingBudgetAndCreationAccounting)
{
    const auto buffer = Supply();
    const auto image  = Supply(1, 2, 1, 1, false, RenderImageLayout{4, 4});
    ResetUploader({80, 128, 2, 4, 2, 128});
    const auto        lease = Begin();
    FrameGraphBuilder builder;
    const auto        batch = Prepare(lease, builder, {buffer, image});
    ASSERT_EQ(batch.admitted.size(), 2u);
    EXPECT_EQ(uploader->InflightBytes(), 80u);
    EXPECT_EQ(uploader->PendingResourceBytes(), 128u);
    EXPECT_TRUE(std::holds_alternative<GraphBufferHandle>(uploader->ResolveInGraph(batch.id, buffer, builder)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(uploader->ResolveInGraph(batch.id, image, builder)));
    auto recording = Record(lease, Freeze(builder));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    Complete();
    EXPECT_EQ(Status(buffer).state, RenderResourceState::RESIDENT);
    EXPECT_EQ(Status(image).upload.completedBytes, 16u);
    EXPECT_EQ(uploader->PendingResourceBytes(), 64u);
    Advance(image);
    EXPECT_EQ(Status(image).state, RenderResourceState::RESIDENT);
    EXPECT_EQ(uploader->PendingResourceBytes(), 0u);
}

TEST_P(BufferUploads, FailedOrCancelledEncodingPreservesCompletedBytes)
{
    ResetUploader({20, 64, 1, 4});
    const auto ticket = Supply();
    Advance(ticket);
    for (bool cancel : {false, true}) {
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch    = Prepare(lease, builder, {ticket});
        auto              compiled = context->Compile(lease, Freeze(builder));
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
        auto         plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
        UploadFaults faults(*device);
        if (!cancel)
            faults.FailEncoding();
        const auto dispatched = context->Dispatch(plan);
        ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
        const auto encoding = std::get<EncodingTicket>(dispatched);
        if (cancel)
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->Cancel(encoding)));
        const auto result = Collect(encoding);
        EXPECT_EQ(std::holds_alternative<EncodingCancelled>(result), cancel);
        EXPECT_EQ(std::holds_alternative<EncodingFailed>(result), !cancel);
        faults.Restore();
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(uploader->Discard(batch.id)));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        EXPECT_EQ(Status(ticket).upload.completedBytes, 20u);
        EXPECT_EQ(uploader->InflightBytes(), 0u);
        EXPECT_EQ(catalog->CpuBytes(), 64u);
    }
    Advance(ticket);
    EXPECT_EQ(Status(ticket).upload.completedBytes, 40u);
}

TEST_P(BufferUploads, SparseImportsRequireGapPatchBeforeWholeReadback)
{
    Buffer::Descriptor desc;
    desc.size  = 64;
    desc.usage = BufferUsageFlagBit::TRANSFER_SRC | BufferUsageFlagBit::TRANSFER_DST;
    BufferPtr backing(device->CreateBuffer(desc));
    ASSERT_TRUE(backing);
    {
        const auto                firstLease = Begin();
        std::array<std::byte, 64> bytes;
        bytes.fill(std::byte{1});
        const auto initialized = context->WriteFrameBuffer(firstLease, FrameBufferKind::STAGING, bytes);
        ASSERT_TRUE(std::holds_alternative<FrameBufferSlice>(initialized));
        const auto       &slice = std::get<FrameBufferSlice>(initialized);
        FrameGraphBuilder first;
        const auto        source = first.Import(slice.buffer, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true);
        const auto        target = first.Import(backing, CopyBufferState::COMMON, CopyBufferState::COMMON, false);
        ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(source));
        ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(target));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(
            first.Copy({std::get<GraphBufferHandle>(source), std::get<GraphBufferHandle>(target), slice.offset, 0, 64, true})));
        auto recording = Record(firstLease, Freeze(first));
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
        Complete();
    }
    const auto                lease = Begin();
    std::array<std::byte, 16> patch;
    patch.fill(std::byte{9});
    const auto staging = context->WriteFrameBuffer(lease, FrameBufferKind::STAGING, patch);
    ASSERT_TRUE(std::holds_alternative<FrameBufferSlice>(staging));
    const auto &slice = std::get<FrameBufferSlice>(staging);
    BufferPtr   readback;
    for (bool fill : {false, true}) {
        FrameGraphBuilder builder;
        FrameGraphBuffer  imported{backing, 64};
        imported.initializedRanges = {{0, 16}, {32, 32}};
        const auto target          = builder.Import(imported);
        ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(target));
        if (fill) {
            const auto source = builder.Import(slice.buffer, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true);
            ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(source));
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(
                builder.Copy({std::get<GraphBufferHandle>(source), std::get<GraphBufferHandle>(target), slice.offset, 16, 16})));
        }
        readback         = Readback(builder, std::get<GraphBufferHandle>(target));
        const auto graph = Freeze(builder);
        if (!fill) {
            const auto rejected = context->Compile(lease, graph);
            ASSERT_TRUE(std::holds_alternative<FrameError>(rejected));
            EXPECT_EQ(std::get<FrameError>(rejected).code, FrameErrorCode::COMPILE_FAILED);
        } else {
            auto recording = Record(lease, graph);
            ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
            Complete();
        }
    }
    const auto *mapped = readback->Map();
    ASSERT_TRUE(mapped);
    for (uint32_t i = 0; i < 64; ++i)
        EXPECT_EQ(mapped[i], i >= 16 && i < 32 ? 9u : 1u);
    readback->UnMap();
}

TEST_P(SampledRaster, UploadedMipIsSampledInBothStagesAndResidentIsReusable)
{
    auto shader  = ShaderOf();
    auto sampler = SamplerOf();
    ASSERT_TRUE(shader && sampler);
    const auto        ticket = Supply(0, 1, 1, 1, false, RenderImageLayout{4, 4, 3, 1});
    const auto        lease  = Begin();
    FrameGraphBuilder builder;
    const auto        batch     = Prepare(lease, builder, {ticket});
    const auto        texture   = std::get<GraphImageHandle>(uploader->ResolveInGraph(batch.id, ticket, builder));
    auto              readback  = Raster(builder, Target(builder), {DrawOf(lease, builder, texture, shader, sampler, 1, {0.25f, 0.25f, 1.f, 0.f})});
    auto              recording = Record(lease, Freeze(builder));
    ASSERT_TRUE(recording);
    EXPECT_EQ(recording->GetGraphReceipt()->passes.size(), 5u);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    EXPECT_EQ(Status(ticket).state, RenderResourceState::UPLOAD_SUBMITTED);
    Complete();
    ExpectBytes(*readback, 4);
    EXPECT_EQ(Status(ticket).state, RenderResourceState::RESIDENT);
    const auto frozen = catalog->Freeze(store);
    ASSERT_EQ(frozen.pins.size(), 1u);
    const auto        nextLease = Begin();
    FrameGraphBuilder next;
    const auto        resident = std::get<GraphImageHandle>(Import(next, *frozen.pins.front()->backing));
    readback                   = Raster(next, Target(next), {DrawOf(nextLease, next, resident, shader, sampler)});
    recording                  = Record(nextLease, Freeze(next));
    ASSERT_TRUE(recording);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete();
    ExpectBytes(*readback, 1);
}

TEST_P(SampledRaster, ParallelDrawsPreserveSparseSetsAndSamplerAddressModes)
{
    auto shaders  = std::array{ShaderOf(), ShaderOf(2)};
    auto samplers = std::array{SamplerOf(), SamplerOf(WrapMode::CLAMP_TO_EDGE)};
    ASSERT_TRUE(shaders[0] && shaders[1] && samplers[0] && samplers[1]);
    for (uint32_t workers : {1u, 3u}) {
        uploader.reset();
        context.reset();
        catalog          = std::move(std::get<std::unique_ptr<ResourceCatalog>>(ResourceCatalog::Create({epoch})));
        info.parallelNum = workers;
        context.reset(device->CreateFrameContext(info));
        ResetUploader();
        const auto            request = std::get<ResourceRequest>(catalog->Request(store, Subscriber(0, 1, workers), true));
        RenderResourcePayload payload{Key(), Name("image-v1"), std::vector<uint8_t>(64), {}};
        payload.metadata.image = RenderImageLayout{4, 4};
        for (uint32_t i = 0; i < 64; ++i)
            payload.bytes[i] = static_cast<uint8_t>(10 + ((i / 4) % 4) * 10);
        ASSERT_FALSE(catalog->Supply(request.ticket, payload));
        const auto              lease = Begin();
        FrameGraphBuilder       builder;
        const auto              batch   = Prepare(lease, builder, {request.ticket});
        const auto              texture = std::get<GraphImageHandle>(uploader->ResolveInGraph(batch.id, request.ticket, builder));
        std::vector<RasterDraw> draws;
        for (uint32_t i = 0; i < 70; ++i) {
            auto draw    = DrawOf(lease, builder, texture, shaders[i % 2], samplers[i % 2], i % 2 + 1, {1.25f, 0.25f, 0.f, 0.f});
            draw.scissor = {{static_cast<int32_t>(i % 2 * 2), 0}, {2, 4}};
            draws.push_back(std::move(draw));
        }
        auto readback  = Raster(builder, Target(builder), std::move(draws));
        auto recording = Record(lease, Freeze(builder));
        ASSERT_TRUE(recording);
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
        Complete();
        const auto *bytes = readback->Map();
        ASSERT_TRUE(bytes);
        for (uint32_t i = 0; i < 64; ++i)
            EXPECT_EQ(bytes[i], ((i / 4) % 4) < 2 ? 20 : 40);
        readback->UnMap();
    }
}

TEST_P(SampledRaster, SampledProducerSurvivesCullingAndMergedPassesShareReadBarrier)
{
    auto shader  = ShaderOf();
    auto sampler = SamplerOf();
    ASSERT_TRUE(shader && sampler);
    const auto        lease = Begin();
    FrameGraphBuilder builder;
    const auto        source = Target(builder);
    FrameRasterPass   producer;
    producer.color = source;
    producer.clear = {42.f / 255.f, 42.f / 255.f, 42.f / 255.f, 42.f / 255.f};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(producer)));
    auto       readback = Raster(builder, Target(builder), {DrawOf(lease, builder, source, shader, sampler)}, true);
    auto       graph    = Freeze(builder);
    const auto analysis = AnalyzeFrameGraph(*graph);
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analysis));
    EXPECT_EQ(std::get<FrameGraphAnalysis>(analysis).livePasses, (std::vector<uint32_t>{0, 1, 2, 3}));
    auto recording = Record(lease, std::move(graph));
    ASSERT_TRUE(recording);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete();
    ExpectBytes(*readback, 42);
}

TEST_P(SampledRaster, InvalidBindingsAndUndefinedMipsFailBeforeEncoding)
{
    auto shader  = ShaderOf();
    auto sampler = SamplerOf();
    ASSERT_TRUE(shader && sampler);
    const auto        ticket = Supply(0, 1, 1, 1, false, RenderImageLayout{4, 4, 2, 1});
    const auto        lease  = Begin();
    FrameGraphBuilder builder;
    const auto        batch    = Prepare(lease, builder, {ticket});
    const auto        texture  = std::get<GraphImageHandle>(uploader->ResolveInGraph(batch.id, ticket, builder));
    auto              readback = Raster(builder, Target(builder), {DrawOf(lease, builder, texture, shader, sampler)});
    const auto        original = Freeze(builder);
    class UnknownSampler final : public Sampler {};
    for (uint32_t fault = 0; fault < 11; ++fault) {
        auto  data = original->GetData();
        auto &pass = std::get<FrameRasterPass>(data.passes[2]);
        auto &draw = pass.draws.front();
        auto  bindings = ResolveRasterBindings(draw);
        if (fault == 0)
            bindings.images.front().image = pass.color;
        if (fault == 1)
            bindings.images.front().image.graph++;
        if (fault == 2)
            bindings.samplers.front().binding = 1;
        if (fault == 3)
            bindings.images.front().set = 3;
        if (fault == 4)
            bindings.samplers.front().sampler.Reset(nullptr);
        if (fault == 5)
            bindings.images.clear();
        if (fault == 6) {
            bindings.images.front().binding   = 2;
            bindings.samplers.front().binding = 1;
        }
        if (fault == 7)
            bindings.samplers.front().sampler.Reset(new UnknownSampler);
        if (fault == 8)
            data.passes.erase(data.passes.begin() + 1);
        if (fault == 9)
            data.images[texture.index].descriptor.arrayLayers = 2;
        if (fault == 10)
            data.images[texture.index].descriptor.usage = ImageUsageFlagBit::TRANSFER_DST;
        const auto bound = SetRasterBindings(draw, std::move(bindings));
        if (const auto *error = std::get_if<FrameError>(&bound)) {
            EXPECT_EQ(error->code, FrameErrorCode::INVALID_ARGUMENT);
            continue;
        }
        auto result = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(original->GetId(), std::move(data)));
        EXPECT_TRUE(std::holds_alternative<FrameError>(result)) << fault;
    }
    auto recording = Record(lease, original);
    ASSERT_TRUE(recording);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    Complete();
    ExpectBytes(*readback, 1);
}

TEST_P(SampledRaster, TwoInflightVersionsRetainTexturesSamplersAndDescriptorsAfterPrune)
{
    auto upload = [&](ResourceTicket ticket) {
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch     = Prepare(lease, builder, {ticket});
        auto              recording = Record(lease, Freeze(builder));
        EXPECT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
        Complete();
    };
    upload(Supply());
    auto old = catalog->Freeze(store);
    ASSERT_EQ(old.pins.size(), 1u);
    Apply({UpdateRenderEntity{Subscriber().entity, Desc(3, 2), 1}});
    upload(Supply(0, 3, 2, 2));
    auto current = catalog->Freeze(store);
    ASSERT_EQ(current.pins.size(), 1u);
    EXPECT_EQ(catalog->Prune(store), 1u);
    auto shader  = ShaderOf();
    auto sampler = SamplerOf();
    ASSERT_TRUE(shader && sampler);
    std::array<BufferPtr, 2>                      readbacks;
    std::array<std::unique_ptr<RecordedFrame>, 2> recordings;
    std::array<std::weak_ptr<const void>, 2>      weak;
    for (uint32_t i = 0; i < 2; ++i) {
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto       &backing = *(i ? current : old).pins.front()->backing;
        const auto        texture = std::get<GraphImageHandle>(Import(builder, backing));
        readbacks[i]              = Raster(builder, Target(builder), {DrawOf(lease, builder, texture, shader, sampler)});
        auto graph                = Freeze(builder);
        weak[i]                   = TrackGraphGpuPayload(graph);
        recordings[i]             = Record(lease, std::move(graph));
        ASSERT_TRUE(recordings[i]);
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recordings[i])));
    }
    old     = {};
    current = {};
    ASSERT_FALSE(catalog->Cancel(Subscriber(0, 2, 2)));
    EXPECT_EQ(catalog->Prune(store), 1u);
    shader.Reset(nullptr);
    sampler.Reset(nullptr);
    EXPECT_FALSE(weak[0].expired());
    EXPECT_FALSE(weak[1].expired());
    Complete(2);
    ExpectBytes(*readbacks[0], 1);
    ExpectBytes(*readbacks[1], 3);
    EXPECT_TRUE(weak[0].expired());
    EXPECT_TRUE(weak[1].expired());
}

TEST_P(SampledRaster, DescriptorExhaustionRollsBackAndRetriesSameLease)
{
    auto shader  = ShaderOf();
    auto sampler = SamplerOf();
    ASSERT_TRUE(shader && sampler);
    const auto ticket = Supply();
    for (bool samplerLimit : {false, true}) {
        uploader.reset();
        context.reset();
        info.allocations.resourceDescriptors = samplerLimit ? 16 : 3;
        info.allocations.samplerDescriptors  = samplerLimit ? 1 : 16;
        context.reset(device->CreateFrameContext(info));
        ResetUploader();
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch    = Prepare(lease, builder, {ticket});
        const auto        texture  = std::get<GraphImageHandle>(uploader->ResolveInGraph(batch.id, ticket, builder));
        auto              draw     = DrawOf(lease, builder, texture, shader, sampler);
        auto              readback = Raster(builder, Target(builder), {draw});
        const auto        original = Freeze(builder);
        auto              data     = original->GetData();
        // Distinct binding identity must still consume capacity after identical sets are shared.
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(
            EditBindings(draw, [&](auto &bindings) { bindings.samplers.front().sampler = SamplerOf(); })));
        std::get<FrameRasterPass>(data.passes[1]).draws.push_back(draw);
        const auto failed = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(original->GetId(), std::move(data)));
        ASSERT_TRUE(std::holds_alternative<FrameError>(failed));
        auto recording = Record(lease, original);
        ASSERT_TRUE(recording);
        recording.reset();
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(uploader->Discard(batch.id)));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        EXPECT_EQ(Status(ticket).state, RenderResourceState::CPU_READY);
    }
}

TEST_P(SampledViews, SelectedMipsDefineReadsAndCullOtherProducers)
{
    auto shader  = ShaderOf();
    auto sampler = SamplerOf();
    ASSERT_TRUE(shader && sampler);
    const auto        lease = Begin();
    FrameGraphBuilder builder;
    const auto        texture = Texture(builder);
    UploadMip(lease, builder, texture, 0, 0, 1);
    UploadMip(lease, builder, texture, 1, 0, 4);
    UploadMip(lease, builder, texture, 2, 0, 7);
    auto draw                = DrawOf(lease, builder, texture, shader, sampler, 1, {0.25f, 0.25f, 1.f, 0.f});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(
        EditBindings(draw, [&](auto &bindings) { bindings.images.front().view = {ImageViewType::VIEW_2D, 1, 2, 0, 1}; })));
    auto       readback      = Raster(builder, Target(builder), {draw});
    const auto graph         = Freeze(builder);
    const auto analysis      = AnalyzeFrameGraph(*graph);
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analysis));
    EXPECT_EQ(std::get<FrameGraphAnalysis>(analysis).livePasses, (std::vector<uint32_t>{1, 2, 3, 4}));
    auto recording = Record(lease, graph);
    ASSERT_TRUE(recording);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete();
    ExpectBytes(*readback, 7);
}

TEST_P(SampledViews, ArrayViewRangesAndRelativeLayersSurviveParallelFragments)
{
    auto shader  = ShaderOf(1, true);
    auto sampler = SamplerOf();
    ASSERT_TRUE(shader && sampler);
    for (uint32_t workers : {1u, 3u}) {
        uploader.reset();
        context.reset();
        info.parallelNum                    = workers;
        info.allocations.samplerDescriptors = 160;
        info.allocations.descriptorSets     = 320;
        context.reset(device->CreateFrameContext(info));
        ResetUploader();
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        texture = Texture(builder, 4);
        UploadMip(lease, builder, texture, 1, 1, 17);
        UploadMip(lease, builder, texture, 2, 2, 33);
        UploadMip(lease, builder, texture, 2, 3, 46);
        std::vector<RasterDraw> draws;
        for (uint32_t i = 0; i < 70; ++i) {
            auto draw = DrawOf(lease, builder, texture, shader, sampler, 1, {0.25f, 0.25f, 20.f, i % 2 ? 1.f : 0.f});
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(EditBindings(draw, [&](auto &bindings) {
                bindings.images.front().view =
                    i % 2 ? RasterImageView{ImageViewType::VIEW_2D_ARRAY, 2, 1, 2, 2} : RasterImageView{ImageViewType::VIEW_2D_ARRAY, 1, 1, 1, 1};
            })));
            draw.scissor = {{static_cast<int32_t>(i % 2 * 2), 0}, {2, 4}};
            draws.push_back(std::move(draw));
        }
        auto readback  = Raster(builder, Target(builder), std::move(draws), true);
        auto recording = Record(lease, Freeze(builder));
        ASSERT_TRUE(recording);
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
        Complete();
        const auto *bytes = readback->Map();
        ASSERT_TRUE(bytes);
        for (uint32_t i = 0; i < 64; ++i)
            EXPECT_EQ(bytes[i], ((i / 4) % 4) < 2 ? 17 : 46);
        readback->UnMap();
    }
}

TEST_P(SampledViews, InvalidRangesMissingContentsAndShaderShapeFailBeforeEncoding)
{
    auto shader       = ShaderOf(1, true);
    auto scalarShader = ShaderOf();
    auto sampler      = SamplerOf();
    ASSERT_TRUE(shader && scalarShader && sampler);
    const auto        lease = Begin();
    FrameGraphBuilder builder;
    const auto        texture = Texture(builder, 3);
    UploadMip(lease, builder, texture, 1, 1, 17);
    auto draw                = DrawOf(lease, builder, texture, shader, sampler);
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(
        EditBindings(draw, [&](auto &bindings) { bindings.images.front().view = {ImageViewType::VIEW_2D_ARRAY, 1, 1, 1, 1}; })));
    auto       readback      = Raster(builder, Target(builder), {draw});
    const auto graph         = Freeze(builder);
    for (uint32_t fault = 0; fault < 11; ++fault) {
        auto  data    = graph->GetData();
        auto &changed = std::get<FrameRasterPass>(data.passes[1]).draws.front();
        auto  bindings = ResolveRasterBindings(changed);
        auto &view     = bindings.images.front().view;
        if (fault == 0)
            view.baseMip = 3;
        if (fault == 1)
            view.baseLayer = 3;
        if (fault == 2)
            view.mipCount = UINT32_MAX;
        if (fault == 3)
            view.layerCount = UINT32_MAX;
        if (fault == 4)
            view.layerCount = 0;
        if (fault == 5)
            view.type = ImageViewType::VIEW_CUBE;
        if (fault == 6)
            view.type = ImageViewType::VIEW_2D;
        if (fault == 7)
            view.mipCount = 0;
        if (fault == 8)
            view.layerCount = 2;
        if (fault == 9)
            changed.shader = scalarShader;
        if (fault == 10)
            data.imageExports.push_back(texture);
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetRasterBindings(changed, std::move(bindings))));
        EXPECT_TRUE(
            std::holds_alternative<FrameError>(context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)))))
            << fault;
    }
    auto recording = Record(lease, graph);
    ASSERT_TRUE(recording);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete();
    ExpectBytes(*readback, 17);
}

TEST_P(SampledViews, ResolverRejectsUnsupportedShaderResourcesBeforeNativeShaderCreation)
{
    const std::array<std::pair<std::string, std::string>, 10> cases{
        {{"Texture2D<float3> image; SamplerState smp;", "return float4(image.SampleLevel(smp, float2(0.5), 0), 1);"},
         {"Texture2D<uint4> image;", "return float4(image.Load(int3(0, 0, 0)));"},
         {"Texture2DMS<float4> image;", "return image.Load(int2(0, 0), 0);"},
         {"Texture3D<float4> image; SamplerState smp;", "return image.SampleLevel(smp, float3(0.5), 0);"},
         {"TextureCube<float4> image; SamplerState smp;", "return image.SampleLevel(smp, float3(1, 0, 0), 0);"},
         {"Texture2D<float4> image; SamplerComparisonState smp;", "return image.SampleCmpLevelZero(smp, float2(0.5), 0.5).xxxx;"},
         {"Texture2D<float4> image[1]; SamplerState smp;", "return image[0].SampleLevel(smp, float2(0.5), 0);"},
         {"Texture2D<float4> image[2]; SamplerState smp;", "return image[1].SampleLevel(smp, float2(0.5), 0);"},
         {"RWTexture2D<float4> image;", "return image[int2(0, 0)];"},
         {"RWStructuredBuffer<float4> buffer;", "return buffer[0];"}}};
    for (const auto &[declaration, body] : cases) {
        const std::string code   = declaration + R"(
            float4 VSMain(uint id : SV_VertexID) : SV_Position { return float4(0, 0, 0, 1); }
            float4 FSMain() : SV_Target { )" +
                                   body + " }";
        const auto        result = ResolveSource(code);
        const auto       *error  = std::get_if<FrameError>(&result);
        ASSERT_TRUE(error) << declaration;
        EXPECT_TRUE(error->code == FrameErrorCode::UNSUPPORTED || error->code == FrameErrorCode::COMPILE_FAILED)
            << declaration << ": " << error->message;
    }
}

TEST_P(SampledViews, PartialImportedOverlappingViewsSampleInSerialAndParallel)
{
    auto shader  = ShaderOf(1, true);
    auto sampler = SamplerOf();
    ASSERT_TRUE(shader && sampler);
    FrameGraphImage image;
    image.descriptor.format      = PixelFormat::RGBA8_UNORM;
    image.descriptor.extent      = {4, 4, 1};
    image.descriptor.mipLevels   = 3;
    image.descriptor.arrayLayers = 3;
    image.descriptor.usage       = ImageUsageFlagBit::TRANSFER_DST | ImageUsageFlagBit::SAMPLED;
    image.image                  = ImagePtr(device->CreateImage(image.descriptor));
    ASSERT_TRUE(image.image);
    image.final = ImageLayout::GENERAL;
    {
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        imported = builder.Import(image);
        ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(imported));
        const auto handle = std::get<GraphImageHandle>(imported);
        UploadMip(lease, builder, handle, 1, 1, 61);
        UploadMip(lease, builder, handle, 2, 1, 97);
        const auto graph = Freeze(builder);
        auto       data  = graph->GetData();
        for (auto &pass : data.passes)
            std::get<BufferImageCopyPass>(pass).sideEffect = true;
        auto recording = Record(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
        ASSERT_TRUE(recording);
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
        Complete();
    }
    image.initial           = ImageLayout::GENERAL;
    image.initializedRanges = {{1, 2, 1, 1, AspectFlagBit::COLOR_BIT}};
    for (const auto workers : {1u, 3u}) {
        uploader.reset();
        context.reset();
        info.parallelNum = workers;
        context.reset(device->CreateFrameContext(info));
        ResetUploader();
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        imported = builder.Import(image);
        ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(imported));
        const auto              handle = std::get<GraphImageHandle>(imported);
        std::vector<RasterDraw> draws;
        for (uint32_t i = 0; i < 70; ++i) {
            auto draw = DrawOf(lease, builder, handle, shader, sampler);
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(EditBindings(draw, [&](auto &bindings) {
                bindings.images.front().view =
                    i % 2 ? RasterImageView{ImageViewType::VIEW_2D_ARRAY, 2, 1, 1, 1} : RasterImageView{ImageViewType::VIEW_2D_ARRAY, 1, 2, 1, 1};
            })));
            draw.scissor = {{static_cast<int32_t>(i % 2 * 2), 0}, {2, 4}};
            draws.push_back(std::move(draw));
        }
        auto readback  = Raster(builder, Target(builder), std::move(draws));
        auto recording = Record(lease, Freeze(builder));
        ASSERT_TRUE(recording);
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
        Complete();
        const auto *bytes = readback->Map();
        ASSERT_TRUE(bytes);
        for (uint32_t i = 0; i < 64; ++i)
            EXPECT_EQ(bytes[i], ((i / 4) % 4) < 2 ? 61 : 97);
        readback->UnMap();
    }
}

TEST_P(VulkanSampledViews, ViewsShareWithinPlanAndRetireAfterFailureDrainOrCompletion)
{
    auto shader  = ShaderOf();
    auto sampler = SamplerOf();
    ASSERT_TRUE(shader && sampler);
    for (uint32_t outcome = 0; outcome < 4; ++outcome) {
        uploader.reset();
        context.reset();
        context.reset(device->CreateFrameContext(info));
        ResetUploader();
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        texture = Texture(builder);
        UploadMip(lease, builder, texture, 1, 0, 4);
        UploadMip(lease, builder, texture, 2, 0, 7);
        std::vector<RasterDraw> draws;
        for (uint32_t i = 0; i < 70; ++i) {
            auto draw                = DrawOf(lease, builder, texture, shader, sampler);
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(
                EditBindings(draw, [&](auto &bindings) { bindings.images.front().view = {ImageViewType::VIEW_2D, i % 2 + 1, 1, 0, 1}; })));
            draws.push_back(std::move(draw));
        }
        auto             readback = Raster(builder, Target(builder), std::move(draws));
        auto             graph    = Freeze(builder);
        SampledViewProbe views(*static_cast<VulkanDevice *>(device));
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
        views.frozen          = true;
        const auto dispatched = context->Dispatch(*plan);
        ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
        const auto ticket = std::get<EncodingTicket>(dispatched);
        if (outcome == 1)
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->Cancel(ticket)));
        auto collected = Collect(ticket);
        if (outcome == 1) {
            EXPECT_TRUE(std::holds_alternative<EncodingCancelled>(collected));
            plan->reset();
            EXPECT_TRUE(views.live.empty());
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        } else {
            auto *recording = std::get_if<std::unique_ptr<RecordedFrame>>(&collected);
            ASSERT_TRUE(recording);
            UploadFaults faults(*device);
            if (outcome == 2)
                faults.UncertainSubmit();
            const auto submitted = context->Submit(*recording);
            faults.Restore();
            EXPECT_EQ(views.live.size(), 2u);
            if (outcome == 2) {
                EXPECT_TRUE(std::holds_alternative<SubmissionIndeterminate>(submitted));
            } else {
                ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(submitted));
                device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
                EXPECT_EQ(views.live.size(), 2u);
                if (outcome == 3) {
                    faults.FailQuery();
                    std::array<FrameCompletion, 2> completions;
                    ASSERT_EQ(context->PollCompleted(completions), 1u);
                    EXPECT_NE(completions[0].status, FrameCompletionStatus::COMPLETE);
                    faults.Restore();
                    EXPECT_EQ(views.live.size(), 2u);
                } else {
                    Complete();
                    ExpectBytes(*readback, 7);
                    EXPECT_TRUE(views.live.empty());
                }
            }
        }
        uploader.reset();
        context.reset();
        EXPECT_TRUE(views.live.empty());
        EXPECT_EQ(views.destroyed, 3u);
        EXPECT_EQ(views.lateCreates, 0u);
    }
}

INSTANTIATE_TEST_SUITE_P(VulkanModes, VulkanSampledViews, testing::Values(3u, 4u));

INSTANTIATE_TEST_SUITE_P(Backends, SampledViews, testing::Values(3u, 4u, 5u));

INSTANTIATE_TEST_SUITE_P(Backends, SampledRaster, testing::Values(3u, 4u, 5u));

INSTANTIATE_TEST_SUITE_P(BackendsAndKinds, ResourceUploads, testing::Values(0u, 1u, 2u, 3u, 4u, 5u));
INSTANTIATE_TEST_SUITE_P(Backends, ImageUploads, testing::Values(3u, 4u, 5u));
INSTANTIATE_TEST_SUITE_P(BackendsAndKinds, ChunkUploads, testing::Values(0u, 1u, 2u, 3u, 4u, 5u));

TEST_P(TrackedUploads, ResidentAndFrozenPinsOwnTrackingAfterUploaderAndCatalogExit)
{
    const auto        ticket = Supply();
    const auto        lease  = Begin();
    FrameGraphBuilder builder;
    const auto        batch  = Prepare(lease, builder, {ticket});
    auto              graph  = Freeze(builder);
    const auto        target = Target(*graph);
    EXPECT_EQ(Snapshot(target).accepted.submission.serial, 0u);
    auto       recording = Record(lease, graph);
    const auto submitted = uploader->Submit(batch.id, recording);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(submitted));
    const auto token = std::get<SubmissionAccepted>(submitted).token;
    EXPECT_EQ(Snapshot(target).accepted.submission, token);
    EXPECT_EQ(Snapshot(target).completed.submission.serial, 0u);
    Complete();
    auto frozen = catalog->Freeze(store);
    ASSERT_EQ(frozen.pins.size(), 1u);
    EXPECT_TRUE(std::visit([](const auto &backing) { return bool(backing.tracking); }, *frozen.pins.front()->backing));
    uploader.reset();
    catalog.reset();
    context->PollCompleted({});
    EXPECT_EQ(Snapshot(target).completed.submission, token);
    FrameGraphBuilder next;
    const auto        nextLease = Begin();
    auto              readback  = Readback(next, Import(next, *frozen.pins.front()->backing));
    recording                   = Record(nextLease, Freeze(next));
    frozen.pins.clear();
    context->PollCompleted({});
    EXPECT_EQ(Snapshot(target).accepted.submission, token);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    context->PollCompleted({});
    EXPECT_TRUE(std::holds_alternative<FrameResourceStateSnapshot>(context->GetResourceState(target)));
    device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
    std::array<FrameCompletion, 2> completed;
    EXPECT_EQ(context->PollCompleted(completed), 1u);
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->GetResourceState(target)));
    ExpectBytes(*readback, 1);
}

TEST_P(TrackedUploads, LedgerCapacityDefersUntilResidentAndSnapshotOwnersRetire)
{
    TrackingLimit(1);
    Apply({UpdateRenderEntity{Subscriber().entity, Desc(3, 2), 1}});
    const auto a = Supply(0, 3, 2, 2), b = Supply(1, 2);
    Advance(a);
    auto frozen = catalog->Freeze(store);
    ASSERT_EQ(frozen.pins.size(), 1u);
    for (uint32_t round = 0; round < 3; ++round) {
        if (round == 1) {
            ASSERT_FALSE(catalog->Cancel(Subscriber(0, 2, 2)));
            EXPECT_EQ(catalog->Prune(store), 1u);
        }
        if (round == 2)
            frozen.pins.clear();
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch = Prepare(lease, builder, {b});
        if (round < 2) {
            ASSERT_EQ(batch.deferred.size(), 1u);
            EXPECT_EQ(batch.deferred.front().reason, UploadDeferReason::STATE_TRACKING);
            EXPECT_TRUE(batch.admitted.empty());
            EXPECT_EQ(Status(b).state, RenderResourceState::CPU_READY);
            EXPECT_EQ(Status(b).upload.submittedBytes, 0u);
        } else {
            ASSERT_EQ(batch.admitted.size(), 1u);
            EXPECT_TRUE(batch.deferred.empty());
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(uploader->Discard(batch.id)));
        }
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    }
}

TEST_P(TrackedUploads, DiscardAndOwnerExitDeferTrackingRetirementUntilPlanDrain)
{
    TrackingLimit(1);
    const auto ticket = Supply();
    for (bool destroy : {false, true}) {
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch    = Prepare(lease, builder, {ticket});
        auto              graph    = Freeze(builder);
        const auto        target   = Target(*graph);
        auto              compiled = context->Compile(lease, graph);
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
        if (destroy)
            uploader.reset();
        else
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(uploader->Discard(batch.id)));
        context->PollCompleted({});
        EXPECT_EQ(Snapshot(target).accepted.submission.serial, 0u);
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->DiscardUnsubmitted(lease)));
        compiled = FrameError{};
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->GetResourceState(target)));
        EXPECT_EQ(Status(ticket).state, RenderResourceState::CPU_READY);
        EXPECT_EQ(Status(ticket).upload.completedBytes, 0u);
    }
}

TEST_P(TrackedUploads, ContinuationImportsTheAcceptedBoundaryAfterAnInterveningGraph)
{
    ResetUploader({20, 64, 1, 4});
    const RenderImageLayout  imageLayout{4, 4, 2, 1};
    const ImageBoundaryState mixed{{ImageLayout::TRANSFER_SRC, ImageLayout::TRANSFER_DST}};
    const auto               ticket = Supply(0, 1, 1, 1, false, GetParam() < 3 ? std::optional<RenderImageLayout>{} : imageLayout);
    const auto               lease  = Begin();
    FrameGraphBuilder        builder;
    const auto               batch     = Prepare(lease, builder, {ticket});
    const auto               graph     = Freeze(builder);
    const auto               target    = Target(*graph);
    auto                     recording = Record(lease, graph);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    Complete();
    FrameGraphBuilder change;
    const auto        changeLease = Begin();
    if (const auto *buffer = std::get_if<BufferPtr>(&target)) {
        FrameGraphBuffer imported{*buffer, 64, CopyBufferState::COMMON, CopyBufferState::RASTER_READ, false};
        imported.initializedRanges = {{0, 20}};
        const auto handle          = std::get<GraphBufferHandle>(change.Import(imported));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(change.Export({handle, 0, 20})));
    } else {
        auto imported               = graph->GetData().images.front();
        imported.initial            = ImageLayout::GENERAL;
        imported.final              = mixed;
        imported.initializedRegions = {{0, 0, {{0, 0}, {4, 1}}}, {0, 0, {{0, 1}, {1, 1}}}};
        const auto         handle   = std::get<GraphImageHandle>(change.Import(imported));
        Buffer::Descriptor desc;
        desc.size   = 16;
        desc.usage  = BufferUsageFlagBit::TRANSFER_DST;
        desc.memory = GetParam() % 3 == 2 ? MemoryType::GPU_TO_CPU : MemoryType::CPU_ONLY;
        BufferPtr host(device->CreateBuffer(desc));
        ASSERT_TRUE(host);
        const auto      output = std::get<GraphBufferHandle>(change.Import(host, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
        BufferImageCopy region;
        region.subRange    = {0, 0, 1, AspectFlagBit::COLOR_BIT};
        region.imageExtent = {4, 1, 1};
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(change.Copy({output, handle, region, ImageCopyDirection::IMAGE_TO_BUFFER, true})));
    }
    recording = Record(changeLease, Freeze(change));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete();
    const auto boundary = Snapshot(target).accepted;
    EXPECT_EQ(boundary.state, GetParam() < 3 ? FrameResourceBoundaryState{CopyBufferState::RASTER_READ} : FrameResourceBoundaryState{mixed});
    const auto        nextLease = Begin();
    FrameGraphBuilder next;
    const auto        continuation = Prepare(nextLease, next, {ticket});
    const auto        nextGraph    = Freeze(next);
    const auto        nextTarget   = Target(*nextGraph);
    EXPECT_EQ(std::visit([](const auto &value) -> const void * { return value.Get(); }, nextTarget),
              std::visit([](const auto &value) -> const void * { return value.Get(); }, target));
    if (GetParam() < 3)
        EXPECT_EQ(nextGraph->GetData().buffers[std::get<BufferCopyPass>(nextGraph->GetData().passes.front()).destination.index].initial,
                  CopyBufferState::RASTER_READ);
    else
        EXPECT_EQ(nextGraph->GetData().images.front().initial, mixed);
    recording = Record(nextLease, nextGraph);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(continuation.id, recording)));
    Complete();
    EXPECT_EQ(Status(ticket).upload.completedBytes, 40u);
    Advance(ticket);
    Advance(ticket);
    EXPECT_EQ(Status(ticket).state, RenderResourceState::RESIDENT);
    FrameGraphBuilder read;
    const auto        readLease = Begin();
    auto              readback  = Readback(read, Import(read, *catalog->Freeze(store).pins.front()->backing), true, imageLayout);
    recording                   = Record(readLease, Freeze(read));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete();
    if (GetParam() < 3)
        ExpectBytes(*readback, 1);
    else
        ExpectImage(*readback, imageLayout, 1);
}

TEST_P(TrackedUploads, PrepareRollbackReleasesNewTrackingAndPreservesCpuRetry)
{
    TrackingLimit(2);
    const auto        a = Supply(), b = Supply(1, 2);
    const auto        lease = Begin();
    FrameGraphBuilder builder;
    for (uint32_t pass = 0; pass < 1023; ++pass)
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy(BufferCopyPass{})));
    const std::array<ResourceTicket, 2> requests{a, b};
    const auto                          result = uploader->Prepare(lease, builder, requests);
    ASSERT_TRUE(std::holds_alternative<FrameError>(result));
    const auto graph  = Freeze(builder);
    const auto target = Target(*graph, 1023);
    EXPECT_EQ(Status(a).state, RenderResourceState::CPU_READY);
    EXPECT_EQ(Status(b).state, RenderResourceState::CPU_READY);
    EXPECT_EQ(uploader->PendingResourceBytes(), 0u);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->GetResourceState(target)));
    const auto        retryLease = Begin();
    FrameGraphBuilder retry;
    const auto        admitted = Prepare(retryLease, retry, {a, b});
    EXPECT_EQ(admitted.admitted.size(), 2u);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(uploader->Discard(admitted.id)));
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(retryLease)));
}

TEST_P(TrackedUploads, DestroyedSubmittedOwnerRetainsTrackingUntilCompletion)
{
    const auto        ticket = Supply();
    const auto        lease  = Begin();
    FrameGraphBuilder builder;
    const auto        batch     = Prepare(lease, builder, {ticket});
    const auto        graph     = Freeze(builder);
    const auto        target    = Target(*graph);
    auto              recording = Record(lease, graph);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    uploader.reset();
    context->PollCompleted({});
    EXPECT_NE(Snapshot(target).accepted.submission.serial, 0u);
    EXPECT_EQ(Status(ticket).state, RenderResourceState::FAILED);
    device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
    std::array<FrameCompletion, 1> completed;
    EXPECT_EQ(context->PollCompleted(completed), 1u);
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->GetResourceState(target)));
}

TEST_P(TrackedUploads, AFailedCompletionPreventsPublicationOfEarlierSuccessInTheSamePoll)
{
    const auto a = Supply(), b = Supply(1, 2);
    for (const auto &ticket : {a, b}) {
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        batch     = Prepare(lease, builder, {ticket});
        auto              recording = Record(lease, Freeze(builder));
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(uploader->Submit(batch.id, recording)));
    }
    device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
    UploadFaults faults(*device);
    faults.FailSecondQuery();
    std::array<FrameCompletion, 2> completed;
    ASSERT_EQ(uploader->PollCompleted(completed), 2u);
    faults.Restore();
    EXPECT_EQ(completed[0].status, FrameCompletionStatus::COMPLETE);
    EXPECT_EQ(completed[1].status, FrameCompletionStatus::DEVICE_LOST);
    EXPECT_EQ(Status(a).state, RenderResourceState::FAILED);
    EXPECT_EQ(Status(b).state, RenderResourceState::FAILED);
    EXPECT_EQ(uploader->InflightBytes(), 64u);
    EXPECT_TRUE(catalog->Freeze(store).pins.empty());
}

INSTANTIATE_TEST_SUITE_P(BackendsAndKinds, TrackedUploads, testing::Values(0u, 1u, 2u, 3u, 4u, 5u));
INSTANTIATE_TEST_SUITE_P(Backends, BufferUploads, testing::Values(0u, 1u, 2u));

TEST_P(SampledViews, RectangleUnionDefinesSampledViewInSerialAndParallel)
{
    auto shader  = ShaderOf(1, true);
    auto sampler = SamplerOf();
    ASSERT_TRUE(shader && sampler);
    for (const auto workers : {1u, 3u}) {
        uploader.reset();
        context.reset();
        info.parallelNum = workers;
        context.reset(device->CreateFrameContext(info));
        ASSERT_TRUE(context);
        ResetUploader();
        const auto        lease = Begin();
        FrameGraphBuilder builder;
        const auto        texture = Texture(builder, 2);
        UploadMip(lease, builder, texture, 1, 1, 61);
        std::vector<RasterDraw> draws;
        for (uint32_t i = 0; i < 70; ++i) {
            auto draw                = DrawOf(lease, builder, texture, shader, sampler);
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(
                EditBindings(draw, [&](auto &bindings) { bindings.images.front().view = {ImageViewType::VIEW_2D_ARRAY, 1, 1, 1, 1}; })));
            draws.push_back(std::move(draw));
        }
        auto       readback = Raster(builder, Target(builder), std::move(draws));
        const auto graph    = Freeze(builder);
        ASSERT_TRUE(graph);
        auto data                     = graph->GetData();
        auto left                     = std::get<BufferImageCopyPass>(data.passes.front());
        left.region.imageExtent.width = 1;
        left.region.bufferRowLength   = 2;
        auto right                    = left;
        right.region.imageOffset.x    = 1;
        right.region.bufferOffset += 4;
        data.passes.front() = left;
        EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(graph->GetId(), data))));
        data.passes.insert(data.passes.begin() + 1, right);
        auto recording = Record(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
        ASSERT_TRUE(recording);
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
        Complete();
        const auto *bytes = readback->Map();
        ASSERT_TRUE(bytes);
        for (uint32_t i = 0; i < 64; ++i)
            EXPECT_EQ(bytes[i], 61);
        readback->UnMap();
    }
}

TEST_P(SampledViews, ImportedRectangleAndNewPatchDefineViewAcrossFrames)
{
    auto shader  = ShaderOf(1, true);
    auto sampler = SamplerOf();
    ASSERT_TRUE(shader && sampler);
    for (const auto workers : {1u, 3u}) {
        uploader.reset();
        context.reset();
        info.parallelNum = workers;
        context.reset(device->CreateFrameContext(info));
        ASSERT_TRUE(context);
        ResetUploader();
        FrameGraphImage image;
        image.descriptor.format      = PixelFormat::RGBA8_UNORM;
        image.descriptor.extent      = {4, 4, 1};
        image.descriptor.mipLevels   = 3;
        image.descriptor.arrayLayers = 2;
        image.descriptor.usage       = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
        image.image                  = ImagePtr(device->CreateImage(image.descriptor));
        ASSERT_TRUE(image.image);
        image.final = ImageLayout::GENERAL;
        {
            const auto        lease = Begin();
            FrameGraphBuilder builder;
            const auto        imported = builder.Import(image);
            ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(imported));
            UploadMip(lease, builder, std::get<GraphImageHandle>(imported), 1, 1, 61);
            const auto graph = Freeze(builder);
            ASSERT_TRUE(graph);
            auto  data                    = graph->GetData();
            auto &copy                    = std::get<BufferImageCopyPass>(data.passes.front());
            copy.region.imageExtent.width = 1;
            copy.region.bufferRowLength   = 2;
            copy.sideEffect               = true;
            auto recording                = Record(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
            ASSERT_TRUE(recording);
            ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
            Complete();
        }
        image.initial            = ImageLayout::GENERAL;
        image.initializedRegions = {{1, 1, {{0, 0}, {1, 2}}}};
        const auto        lease  = Begin();
        FrameGraphBuilder builder;
        const auto        imported = builder.Import(image);
        ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(imported));
        const auto texture = std::get<GraphImageHandle>(imported);
        UploadMip(lease, builder, texture, 1, 1, 97);
        std::vector<RasterDraw> draws;
        for (uint32_t i = 0; i < 70; ++i) {
            auto draw                = DrawOf(lease, builder, texture, shader, sampler, 1, {i % 2 ? 0.75f : 0.25f, 0.25f, 0.f, 0.f});
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(
                EditBindings(draw, [&](auto &bindings) { bindings.images.front().view = {ImageViewType::VIEW_2D_ARRAY, 1, 1, 1, 1}; })));
            draw.scissor             = {{static_cast<int32_t>(i % 2 * 2), 0}, {2, 4}};
            draws.push_back(std::move(draw));
        }
        auto       readback = Raster(builder, Target(builder), std::move(draws));
        const auto graph    = Freeze(builder);
        ASSERT_TRUE(graph);
        auto  data                     = graph->GetData();
        auto &right                    = std::get<BufferImageCopyPass>(data.passes.front());
        right.region.imageExtent.width = 1;
        right.region.imageOffset.x     = 1;
        right.region.bufferRowLength   = 2;
        right.region.bufferOffset += 4;
        auto gap                                                              = data;
        gap.images.front().initializedRegions.front().rectangle.extent.height = 1;
        const auto failed = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(gap)));
        EXPECT_TRUE(std::holds_alternative<FrameError>(failed));
        auto recording = Record(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
        ASSERT_TRUE(recording);
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
        Complete();
        const auto *bytes = readback->Map();
        ASSERT_TRUE(bytes);
        for (uint32_t i = 0; i < 64; ++i)
            EXPECT_EQ(bytes[i], (i / 4) % 4 < 2 ? 61 : 97);
        readback->UnMap();
    }
}

TEST_P(ImageUploads, ResolveWriteReceiptCannotPublishAnUploadedVersion)
{
    const auto        ticket = Supply();
    const auto        lease  = Begin();
    FrameGraphBuilder builder;
    const auto        batch     = Prepare(lease, builder, {ticket});
    const auto        graph     = Freeze(builder);
    auto              recording = Record(lease, graph);
    ASSERT_TRUE(recording);
    auto *receipt = const_cast<FrameGraphReceipt *>(recording->GetGraphReceipt());
    ASSERT_TRUE(receipt);
    auto            data   = graph->GetData();
    const auto      target = std::get<BufferImageCopyPass>(data.passes.front()).image;
    FrameGraphImage source;
    source.transient          = true;
    source.descriptor.format  = PixelFormat::RGBA8_UNORM;
    source.descriptor.extent  = data.images[target.index].descriptor.extent;
    source.descriptor.samples = SampleCount::X4;
    source.descriptor.usage   = ImageUsageFlagBit::RENDER_TARGET;
    FrameRasterPass pass;
    pass.color        = {graph->GetId(), static_cast<uint32_t>(data.images.size())};
    pass.colorResolve = RasterColorResolve{target};
    pass.sideEffect   = true;
    data.images.push_back(std::move(source));
    receipt->passes.push_back(static_cast<uint32_t>(data.passes.size()));
    data.passes.emplace_back(pass);
    // Inject an additional claimed write at the receipt boundary without issuing native work.
    receipt->graph = std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data));
    ASSERT_TRUE(std::holds_alternative<SubmissionRejected>(uploader->Submit(batch.id, recording)));
    EXPECT_EQ(Status(ticket).state, RenderResourceState::UPLOAD_QUEUED);
    EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
    recording.reset();
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(uploader->Discard(batch.id)));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    EXPECT_EQ(Status(ticket).state, RenderResourceState::CPU_READY);
}
