//
// Created on 2026/10/15.
//

#include "rdg/D3D12FrameCompiler.h"
#include "GpuTestSupport.h"
#include "graph/FrameGraphBuilder.h"
#include <D3D12Device.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <future>
#include <map>
#include <set>

namespace {
    struct DxProbe {
        enum class Fault { NONE, ALLOCATOR, LIST, RESET_ALLOCATOR, BEGIN, END, PIPELINE, TARGETS };
        struct List {
            std::thread::id owner;
            bool            recording = false;
            uint32_t        first     = UINT32_MAX;
        };
        explicit DxProbe(D3D12Device &device) : fn(const_cast<D3D12Functions &>(device.GetFunctions())), saved(fn)
        {
            active                    = this;
            fn.createAllocator        = CreateAllocator;
            fn.createList             = CreateList;
            fn.resetAllocator         = ResetAllocator;
            fn.resetList              = Begin;
            fn.closeList              = End;
            fn.draw                   = Draw;
            fn.execute                = Execute;
            fn.signal                 = Signal;
            fn.completed              = Completed;
            fn.createGraphicsPipeline = Pipeline;
            fn.createDescriptorHeap   = Targets;
        }
        ~DxProbe()
        {
            fn     = saved;
            active = nullptr;
        }
        bool Fail(Fault point)
        {
            return fault == point && !injected.exchange(true);
        }
        static HRESULT CreateAllocator(ID3D12Device *device, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator **out)
        {
            if (active->Fail(Fault::ALLOCATOR))
                return E_OUTOFMEMORY;
            const auto result = active->saved.createAllocator(device, type, out);
            if (SUCCEEDED(result)) {
                std::lock_guard lock(active->mutex);
                active->allocators[*out] = std::this_thread::get_id();
            }
            return result;
        }
        static HRESULT
        CreateList(ID3D12Device *device, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator *allocator, ID3D12GraphicsCommandList **out)
        {
            if (active->Fail(Fault::LIST))
                return E_OUTOFMEMORY;
            const auto result = active->saved.createList(device, type, allocator, out);
            if (SUCCEEDED(result)) {
                std::lock_guard lock(active->mutex);
                ++active->createdLists;
                active->lists[*out] = {};
            }
            return result;
        }
        static HRESULT ResetAllocator(ID3D12CommandAllocator *allocator)
        {
            {
                std::lock_guard lock(active->mutex);
                EXPECT_FALSE(active->protectedAllocators.contains(allocator));
                if (active->allocators.contains(allocator))
                    EXPECT_EQ(active->allocators[allocator], std::this_thread::get_id());
                ++active->resets;
            }
            return std::this_thread::get_id() != active->coordinator && active->Fail(Fault::RESET_ALLOCATOR)
                       ? E_OUTOFMEMORY
                       : active->saved.resetAllocator(allocator);
        }
        static HRESULT Begin(ID3D12GraphicsCommandList *list, ID3D12CommandAllocator *allocator)
        {
            if (active->Fail(Fault::BEGIN))
                return E_OUTOFMEMORY;
            const auto result = active->saved.resetList(list, allocator);
            if (FAILED(result))
                return result;
            std::unique_lock lock(active->mutex);
            auto            &entry = active->lists[list];
            EXPECT_FALSE(entry.recording);
            entry = {std::this_thread::get_id(), true, UINT32_MAX};
            ++active->concurrent;
            active->maximum = std::max(active->maximum, active->concurrent);
            active->threads.insert(entry.owner);
            ++active->begun;
            active->cv.notify_all();
            if (active->gateBegins && active->begun <= 2)
                EXPECT_TRUE(active->cv.wait_for(lock, 5s, [] { return active->begun >= 2 || active->released; }));
            return result;
        }
        static void Draw(ID3D12GraphicsCommandList *list, UINT vertices, UINT instances, UINT firstVertex, UINT firstInstance)
        {
            {
                std::unique_lock lock(active->mutex);
                auto            &entry = active->lists[list];
                EXPECT_TRUE(entry.recording);
                EXPECT_EQ(entry.owner, std::this_thread::get_id());
                if (entry.first == UINT32_MAX)
                    entry.first = firstInstance;
                if (active->delayFirst && firstInstance == 0)
                    EXPECT_TRUE(active->cv.wait_for(lock, 5s, [] { return active->finished > 0 || active->released; }));
            }
            active->saved.draw(list, vertices, instances, firstVertex, firstInstance);
        }
        static HRESULT End(ID3D12GraphicsCommandList *list)
        {
            std::unique_lock lock(active->mutex);
            auto            &entry = active->lists[list];
            if (!entry.recording)
                return active->saved.closeList(list);
            EXPECT_EQ(entry.owner, std::this_thread::get_id());
            if (active->holdEnd && entry.first != 0) {
                ++active->held;
                active->cv.notify_all();
                EXPECT_TRUE(active->cv.wait_for(lock, 5s, [] { return active->released; }));
            }
            HRESULT failure = S_OK;
            if (active->endFailures.contains(entry.first))
                failure = active->endFailures[entry.first];
            else if (active->Fail(Fault::END))
                failure = E_OUTOFMEMORY;
            entry.recording = false;
            --active->concurrent;
            ++active->finished;
            active->finishOrder.push_back(entry.first);
            active->cv.notify_all();
            lock.unlock();
            return FAILED(failure) ? failure : active->saved.closeList(list);
        }
        static void Execute(ID3D12CommandQueue *queue, UINT count, ID3D12CommandList *const *lists)
        {
            std::lock_guard lock(active->mutex);
            EXPECT_EQ(active->concurrent, 0u);
            EXPECT_EQ(std::this_thread::get_id(), active->coordinator);
            ++active->submits;
            active->order.clear();
            for (UINT i = 0; i < count; ++i)
                active->order.push_back(active->lists[static_cast<ID3D12GraphicsCommandList *>(lists[i])].first);
            active->saved.execute(queue, count, lists);
        }
        static HRESULT Signal(ID3D12CommandQueue *queue, ID3D12Fence *fence, UINT64 value)
        {
            std::unique_lock lock(active->mutex);
            if (active->holdIdle) {
                EXPECT_EQ(active->concurrent, 0u);
                active->idleEntered = true;
                active->cv.notify_all();
                EXPECT_TRUE(active->cv.wait_for(lock, 5s, [] { return active->releaseIdle; }));
            }
            if (active->failSignal) {
                active->failSignal = false;
                return E_OUTOFMEMORY;
            }
            return active->saved.signal(queue, fence, value);
        }
        static UINT64 Completed(ID3D12Fence *fence)
        {
            return active->queryOverride ? active->queryValue : active->saved.completed(fence);
        }
        static HRESULT Pipeline(ID3D12Device *device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc, ID3D12PipelineState **out)
        {
            return active->Fail(Fault::PIPELINE) ? active->compileFailure : active->saved.createGraphicsPipeline(device, desc, out);
        }
        static HRESULT Targets(ID3D12Device *device, const D3D12_DESCRIPTOR_HEAP_DESC *desc, ID3D12DescriptorHeap **out)
        {
            return active->Fail(Fault::TARGETS) ? active->compileFailure : active->saved.createDescriptorHeap(device, desc, out);
        }
        void Release()
        {
            std::lock_guard lock(mutex);
            released = true;
            cv.notify_all();
        }
        D3D12Functions                                     &fn;
        D3D12Functions                                      saved;
        static inline DxProbe                              *active      = nullptr;
        const std::thread::id                               coordinator = std::this_thread::get_id();
        std::mutex                                          mutex;
        std::condition_variable                             cv;
        std::map<ID3D12GraphicsCommandList *, List>         lists;
        std::map<ID3D12CommandAllocator *, std::thread::id> allocators;
        std::set<ID3D12CommandAllocator *>                  protectedAllocators;
        std::set<std::thread::id>                           threads;
        std::map<uint32_t, HRESULT>                         endFailures;
        std::vector<uint32_t>                               finishOrder, order;
        Fault                                               fault          = Fault::NONE;
        HRESULT                                             compileFailure = E_OUTOFMEMORY;
        std::atomic_bool                                    injected{false};
        uint32_t concurrent = 0, maximum = 0, begun = 0, finished = 0, held = 0, submits = 0, createdLists = 0, resets = 0;
        bool     gateBegins = false, delayFirst = false, holdEnd = false, released = false;
        bool     failSignal = false, queryOverride = false, holdIdle = false, idleEntered = false, releaseIdle = false;
        UINT64   queryValue = 0;
    };

    class D3D12CustomComputeEncoder final : public FrameCustomPassEncoder {
    public:
        ComputePipeline     *pipeline = nullptr;
        ResourceGroup       *group    = nullptr;
        FrameOperationResult Encode(FrameCustomPassContext &context) const noexcept override
        {
            auto encoder = context.command.CreateComputeEncoder();
            encoder->BindPipeline(pipeline);
            encoder->BindResourceGroup(0, group);
            encoder->Dispatch(4, 1, 1);
            return FrameOperationComplete{};
        }
    };
    class D3D12FrameCompilerTest : public testing::Test {
    protected:
        void SetUp() override
        {
            Instance::Get()->Init({"D3D12FrameCompilerTest", "SkyEngine", true, API::DX12});
            device = static_cast<D3D12Device *>(Instance::Get()->GetDevice());
            ASSERT_TRUE(device);
            ASSERT_TRUE(SUCCEEDED(device->GetNativeHandle()->QueryInterface(IID_PPV_ARGS(&debug))));
            debug->ClearStoredMessages();
            ResetContext(2);
        }
        void TearDown() override
        {
            context.reset();
            device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
            for (UINT64 i = 0; i < debug->GetNumStoredMessages(); ++i) {
                SIZE_T size = 0;
                debug->GetMessage(i, nullptr, &size);
                std::vector<char> bytes(size);
                auto             *message = reinterpret_cast<D3D12_MESSAGE *>(bytes.data());
                if (SUCCEEDED(debug->GetMessage(i, message, &size)))
                    EXPECT_GT(message->Severity, D3D12_MESSAGE_SEVERITY_ERROR) << message->pDescription;
            }
            debug.Reset();
            Instance::Destroy();
        }
        void ResetContext(uint32_t workers, uint32_t slots = 2, uint32_t budget = 32)
        {
            context.reset(device->CreateFrameContext({slots, workers, 101, budget}));
            ASSERT_TRUE(context);
        }
        ShaderPtr ShaderOf(bool green = false)
        {
            const char *source =
                "float4 VSMain(uint i:SV_VertexID):SV_Position { float2 p[3]={float2(-1,-1),float2(-1,3),float2(3,-1)}; return float4(p[i],0,1); }\n"
                "float4 Red():SV_Target {return float4(1,0,0,1);}\nfloat4 Green():SV_Target {return float4(0,1,0,1);}";
            auto function = [&](ShaderStageFlagBit stage, const char *entry, const char *profile) {
                ComPtr<ID3DBlob> code, errors;
                const auto       result = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, entry, profile, 0, 0, &code, &errors);
                EXPECT_TRUE(SUCCEEDED(result));
                if (FAILED(result))
                    return ShaderFunctionPtr{};
                CounterPtr<ShaderBinaryProvider> provider(new ShaderBinaryProvider());
                provider->binaryData = BinaryDataPtr(new BinaryData(static_cast<uint32_t>(code->GetBufferSize())));
                std::memcpy(provider->binaryData->Data(), code->GetBufferPointer(), code->GetBufferSize());
                return ShaderFunctionPtr(device->CreateShaderFunction({stage, provider, entry}));
            };
            auto               vs = function(ShaderStageFlagBit::VS, "VSMain", "vs_5_0");
            auto               ps = function(ShaderStageFlagBit::FS, green ? "Green" : "Red", "ps_5_0");
            ShaderReflection   reflection;
            Shader::Descriptor desc{};
            desc.vs         = vs.Get();
            desc.ps         = ps.Get();
            desc.reflection = &reflection;
            return ShaderPtr(device->CreateShader(desc));
        }
        FrameGraphImage ImageOf()
        {
            FrameGraphImage image;
            image.descriptor.format = PixelFormat::RGBA8_UNORM;
            image.descriptor.extent = {8, 8, 1};
            image.descriptor.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC | ImageUsageFlagBit::TRANSFER_DST;
            image.image             = ImagePtr(device->CreateImage(image.descriptor));
            return image;
        }
        BufferPtr BufferOf(bool upload, uint64_t size = 256)
        {
            Buffer::Descriptor desc;
            desc.size   = size;
            desc.memory = upload ? MemoryType::CPU_TO_GPU : MemoryType::GPU_TO_CPU;
            desc.usage  = upload ? BufferUsageFlagBit::TRANSFER_SRC : BufferUsageFlagBit::TRANSFER_DST;
            return BufferPtr(device->CreateBuffer(desc));
        }
        BufferPtr StorageBufferOf()
        {
            Buffer::Descriptor desc;
            desc.size   = 64;
            desc.memory = MemoryType::GPU_ONLY;
            desc.usage  = BufferUsageFlagBit::STORAGE | BufferUsageFlagBit::TRANSFER_SRC;
            return BufferPtr(device->CreateBuffer(desc));
        }
        ShaderPtr ComputeShaderOf(bool accumulate = false)
        {
            const char      *source = accumulate ? "RWStructuredBuffer<uint> data : register(u0);\n"
                                                   "[numthreads(1,1,1)] void CSMain(uint3 id:SV_DispatchThreadID){ data[id.x]+=100; }"
                                                 : "RWStructuredBuffer<uint> data : register(u0);\n"
                                                   "[numthreads(1,1,1)] void CSMain(uint3 id:SV_DispatchThreadID){ data[id.x]=id.x+7; }";
            ComPtr<ID3DBlob> code, errors;
            const auto       result = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, "CSMain", "cs_5_0", 0, 0, &code, &errors);
            EXPECT_TRUE(SUCCEEDED(result));
            if (FAILED(result)) {
                if (errors)
                    ADD_FAILURE() << static_cast<const char *>(errors->GetBufferPointer());
                return {};
            }
            CounterPtr<ShaderBinaryProvider> provider(new ShaderBinaryProvider());
            provider->binaryData = BinaryDataPtr(new BinaryData(static_cast<uint32_t>(code->GetBufferSize())));
            std::memcpy(provider->binaryData->Data(), code->GetBufferPointer(), code->GetBufferSize());
            auto             cs = ShaderFunctionPtr(device->CreateShaderFunction({ShaderStageFlagBit::CS, provider, "CSMain"}));
            ShaderReflection reflection;
            ShaderResource   resource;
            resource.name    = "data";
            resource.type    = ShaderResourceType::STORAGE_BUFFER;
            resource.set     = 0;
            resource.binding = 0;
            resource.count   = 1;
            reflection.resources.push_back(resource);
            Shader::Descriptor desc{};
            desc.cs         = cs.Get();
            desc.reflection = &reflection;
            return ShaderPtr(device->CreateShader(desc));
        }
        FrameGraphImage StorageImageOf()
        {
            FrameGraphImage image;
            image.descriptor.format = PixelFormat::RGBA8_UNORM;
            image.descriptor.extent = {8, 8, 1};
            image.descriptor.usage  = ImageUsageFlagBit::STORAGE | ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_SRC;
            image.image             = ImagePtr(device->CreateImage(image.descriptor));
            return image;
        }
        ShaderFunctionPtr CompileFunction(const char *source, ShaderStageFlagBit stage, const char *entry, const char *profile)
        {
            ComPtr<ID3DBlob> code, errors;
            const auto       result = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, entry, profile, 0, 0, &code, &errors);
            EXPECT_TRUE(SUCCEEDED(result));
            if (FAILED(result)) {
                if (errors)
                    ADD_FAILURE() << static_cast<const char *>(errors->GetBufferPointer());
                return {};
            }
            CounterPtr<ShaderBinaryProvider> provider(new ShaderBinaryProvider());
            provider->binaryData = BinaryDataPtr(new BinaryData(static_cast<uint32_t>(code->GetBufferSize())));
            std::memcpy(provider->binaryData->Data(), code->GetBufferPointer(), code->GetBufferSize());
            return ShaderFunctionPtr(device->CreateShaderFunction({stage, provider, entry}));
        }
        ShaderPtr ComputeImageShaderOf()
        {
            const char      *source = "RWTexture2D<float4> outputImage : register(u0);\n"
                                      "[numthreads(1,1,1)] void CSMain(uint3 id:SV_DispatchThreadID){ outputImage[id.xy]=float4(0,1,0,1); }";
            auto             cs     = CompileFunction(source, ShaderStageFlagBit::CS, "CSMain", "cs_5_0");
            ShaderReflection reflection;
            ShaderResource   resource;
            resource.name              = "outputImage";
            resource.type              = ShaderResourceType::STORAGE_IMAGE;
            resource.set               = 0;
            resource.binding           = 0;
            resource.count             = 1;
            resource.texture.dimension = ShaderTextureDimension::D2;
            reflection.resources.push_back(resource);
            Shader::Descriptor desc{};
            desc.cs         = cs.Get();
            desc.reflection = &reflection;
            return ShaderPtr(device->CreateShader(desc));
        }
        ShaderPtr SampleShaderOf()
        {
            const char      *source = "Texture2D<float4> sourceTex : register(t0);\n"
                                      "SamplerState sourceSampler : register(s1);\n"
                                      "float4 VSMain(uint i:SV_VertexID):SV_Position { float2 p[3]={float2(-1,-1),float2(-1,3),float2(3,-1)}; "
                                      "return float4(p[i],0,1); }\n"
                                      "float4 PSMain(float4 pos:SV_Position):SV_Target { return sourceTex.Sample(sourceSampler, pos.xy/8.0); }";
            auto             vs     = CompileFunction(source, ShaderStageFlagBit::VS, "VSMain", "vs_5_0");
            auto             fs     = CompileFunction(source, ShaderStageFlagBit::FS, "PSMain", "ps_5_0");
            ShaderReflection reflection;
            ShaderResource   image;
            image.name               = "sourceTex";
            image.type               = ShaderResourceType::SAMPLED_IMAGE;
            image.set                = 0;
            image.binding            = 0;
            image.count              = 1;
            image.texture.dimension  = ShaderTextureDimension::D2;
            image.texture.scalarType = ShaderScalarType::FLOAT;
            image.texture.components = 4;
            reflection.resources.push_back(image);
            ShaderResource sampler;
            sampler.name    = "sourceSampler";
            sampler.type    = ShaderResourceType::SAMPLER;
            sampler.set     = 0;
            sampler.binding = 1;
            sampler.count   = 1;
            sampler.sampler = ShaderSamplerType::REGULAR;
            reflection.resources.push_back(sampler);
            Shader::Descriptor desc{};
            desc.vs         = vs.Get();
            desc.ps         = fs.Get();
            desc.reflection = &reflection;
            return ShaderPtr(device->CreateShader(desc));
        }
        BufferPtr Readback(FrameGraphBuilder &builder, GraphImageHandle image)
        {
            auto       buffer = BufferOf(false);
            const auto target = std::get<GraphBufferHandle>(builder.Import(buffer, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
            BufferImageCopy region{};
            region.subRange.layers = 1;
            region.imageExtent     = {8, 8, 1};
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(
                builder.Copy(BufferImageCopyPass{target, image, region, ImageCopyDirection::IMAGE_TO_BUFFER})));
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export(GraphBufferExport{target, 0, 256})));
            return buffer;
        }
        std::pair<std::shared_ptr<const FrozenFrameGraph>, BufferPtr> Graph(bool fragmented = true)
        {
            FrameGraphBuilder builder;
            const auto        image = std::get<GraphImageHandle>(builder.Import(ImageOf()));
            const auto        red = ShaderOf(), green = ShaderOf(true);
            FrameRasterPass   pass;
            pass.color           = image;
            const uint32_t count = fragmented ? 65 : 1;
            for (uint32_t i = 0; i < count; ++i)
                pass.draws.push_back({i + 1 == count ? green : red, {}, {3, 1, 0, i}, {}});
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(pass))));
            if (fragmented) {
                for (uint32_t i = 65; i < 70; ++i) {
                    FrameRasterPass next;
                    next.color = image;
                    next.load  = LoadOp::LOAD;
                    next.draws.push_back({i == 69 ? red : green, {}, {3, 1, 0, i}, {{4, 0}, {4, 8}}});
                    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(next))));
                }
            }
            auto buffer = Readback(builder, image);
            return {std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()), std::move(buffer)};
        }
        DeviceFrameLease Begin()
        {
            auto result = context->TryBeginSlot();
            EXPECT_TRUE(std::holds_alternative<DeviceFrameLease>(result));
            return std::holds_alternative<DeviceFrameLease>(result) ? std::get<DeviceFrameLease>(result) : DeviceFrameLease{};
        }
        EncodingTicket Dispatch(std::shared_ptr<const FrozenFrameGraph> graph)
        {
            auto compiled = context->Compile(Begin(), std::move(graph));
            EXPECT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
            if (!std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled))
                return {};
            auto plan   = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
            auto result = context->Dispatch(plan);
            EXPECT_FALSE(plan);
            EXPECT_TRUE(std::holds_alternative<EncodingTicket>(result));
            return std::holds_alternative<EncodingTicket>(result) ? std::get<EncodingTicket>(result) : EncodingTicket{};
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
        void Complete(size_t count = 1)
        {
            const auto deadline  = std::chrono::steady_clock::now() + 5s;
            size_t     completed = 0;
            while (completed < count && std::chrono::steady_clock::now() < deadline) {
                std::array<FrameCompletion, 2> output;
                const auto                     received = context->PollCompleted(output);
                for (size_t i = 0; i < received; ++i)
                    EXPECT_EQ(output[i].status, FrameCompletionStatus::COMPLETE);
                completed += received;
                if (!received)
                    std::this_thread::sleep_for(1ms);
            }
            EXPECT_EQ(completed, count);
        }
        void Execute(std::shared_ptr<const FrozenFrameGraph> graph, const std::function<void(const D3D12CompiledFrame &)> &inspect = {})
        {
            auto compiled = context->Compile(Begin(), std::move(graph));
            ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
            auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
            if (inspect)
                inspect(*static_cast<const D3D12CompiledFrame *>(plan.get()));
            auto dispatched = context->Dispatch(plan);
            ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
            auto result = Collect(std::get<EncodingTicket>(dispatched));
            ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(result));
            auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(result));
            ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
            Complete();
        }
        D3D12Device                        *device = nullptr;
        std::unique_ptr<DeviceFrameContext> context;
        ComPtr<ID3D12InfoQueue>             debug;
    };
} // namespace

TEST_F(D3D12FrameCompilerTest, FullscreenGeneratedTriangleReadsBackThroughOwnCompiler)
{
    FrameGraphBuilder builder;
    auto              image = std::get<GraphImageHandle>(builder.Import(ImageOf()));
    FrameRasterPass   pass;
    pass.color = image;
    pass.draws.push_back({ShaderOf(), {}, {3, 1, 0, 0}, {}});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Fullscreen(std::move(pass))));
    auto readback = Readback(builder, image);
    Execute(std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()));
    auto *pixels = readback->Map();
    ASSERT_TRUE(pixels);
    for (uint32_t i = 0; i < 64; ++i) {
        EXPECT_EQ(pixels[i * 4], 255);
        EXPECT_EQ(pixels[i * 4 + 1], 0);
        EXPECT_EQ(pixels[i * 4 + 2], 0);
        EXPECT_EQ(pixels[i * 4 + 3], 255);
    }
    readback->UnMap();
}

TEST_F(D3D12FrameCompilerTest, ComputeStorageBufferWritesReadBack)
{
    FrameGraphBuilder builder;
    auto              storage = StorageBufferOf();
    const auto        source =
        std::get<GraphBufferHandle>(builder.Import(FrameGraphBuffer{storage, 64, CopyBufferState::COMMON, CopyBufferState::COMMON, false}));
    auto             readback = BufferOf(false);
    const auto       target   = std::get<GraphBufferHandle>(builder.Import(readback, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
    FrameComputePass compute;
    compute.shader  = ComputeShaderOf();
    compute.groupsX = 4;
    compute.buffers.push_back({0, 0, {source, 0, 64}, 0, FrameAccessMode::WRITE});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Compute(std::move(compute))));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy(BufferCopyPass{source, target, 0, 0, 64})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export(GraphBufferExport{target, 0, 64})));
    Execute(std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()));
    auto *values = static_cast<const uint32_t *>(static_cast<const void *>(readback->Map()));
    ASSERT_TRUE(values);
    for (uint32_t i = 0; i < 4; ++i)
        EXPECT_EQ(values[i], i + 7);
    readback->UnMap();
}

TEST_F(D3D12FrameCompilerTest, ComputeStorageImageIsSampledByRaster)
{
    FrameGraphBuilder builder;
    const auto        source = std::get<GraphImageHandle>(builder.Import(StorageImageOf()));
    const auto        output = std::get<GraphImageHandle>(builder.Import(ImageOf()));
    FrameComputePass  compute;
    compute.shader  = ComputeImageShaderOf();
    compute.groupsX = 8;
    compute.groupsY = 8;
    compute.images.push_back({0, 0, source, {}, FrameAccessMode::WRITE});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Compute(std::move(compute))));
    FrameRasterPass raster;
    raster.color = output;
    RasterDraw draw;
    draw.shader  = SampleShaderOf();
    draw.command = {3, 1, 0, 0};
    draw.images.push_back({0, 0, source, {}});
    draw.samplers.push_back({0, 1, SamplerPtr(device->CreateSampler({}))});
    raster.draws.push_back(std::move(draw));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(raster))));
    auto readback = Readback(builder, output);
    Execute(std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()));
    auto *pixels = readback->Map();
    ASSERT_TRUE(pixels);
    for (uint32_t i = 0; i < 64; ++i) {
        EXPECT_EQ(pixels[i * 4], 0);
        EXPECT_EQ(pixels[i * 4 + 1], 255);
        EXPECT_EQ(pixels[i * 4 + 2], 0);
        EXPECT_EQ(pixels[i * 4 + 3], 255);
    }
    readback->UnMap();
}

TEST_F(D3D12FrameCompilerTest, CustomReadsStorageImageAndBuffer)
{
    const char *source =
        "RWTexture2D<float4> image : register(u0); RWStructuredBuffer<uint> output : register(u1); RWStructuredBuffer<uint> input : register(u2);\n"
        "[numthreads(1,1,1)] void CSMain(uint3 id:SV_DispatchThreadID){ output[id.x]=input[id.x]+uint(image[int2(0,0)].g*255); }";
    const auto function = CompileFunction(source, ShaderStageFlagBit::CS, "CSMain", "cs_5_0");
    ASSERT_TRUE(function);
    const auto         reflection = StorageReadReflection();
    Shader::Descriptor desc{};
    desc.cs           = function.Get();
    desc.reflection   = &reflection;
    const auto shader = ShaderPtr(device->CreateShader(desc));
    ASSERT_TRUE(shader);
    auto [graph, readback] = CustomStorageReadGraph(*device, ComputeImageShaderOf(), ComputeShaderOf(), shader);
    ASSERT_TRUE(graph);
    Execute(graph);
    const auto *values = reinterpret_cast<const uint32_t *>(readback->Map());
    ASSERT_TRUE(values);
    for (uint32_t i = 0; i < 4; ++i)
        EXPECT_EQ(values[i], 262u + i);
    readback->UnMap();
}

TEST_F(D3D12FrameCompilerTest, CustomComputeWritesDeclaredBufferReadBack)
{
    auto                        shader = ComputeShaderOf();
    ComputePipeline::Descriptor pipelineDesc{};
    pipelineDesc.cs = shader.Get();
    CounterPtr<ComputePipeline> pipeline(device->CreatePipelineState(pipelineDesc));
    ASSERT_TRUE(pipeline);
    ResourceGroup::Descriptor groupDesc{shader.Get(), 0};
    CounterPtr<ResourceGroup> group(device->CreateResourceGroup(groupDesc));
    ASSERT_TRUE(group);
    auto descriptor = group->CreateEncoder();
    ASSERT_TRUE(descriptor);

    FrameGraphBuilder builder;
    auto              storage = StorageBufferOf();
    const auto        declared =
        std::get<GraphBufferHandle>(builder.Import(FrameGraphBuffer{storage, 64, CopyBufferState::COMMON, CopyBufferState::COMMON, false}));
    descriptor->WriteBuffer(0, storage.Get(), 0, 64, 0);
    descriptor->End();
    auto callback            = std::make_shared<D3D12CustomComputeEncoder>();
    callback->pipeline       = pipeline.Get();
    callback->group          = group.Get();
    auto            readback = BufferOf(false);
    const auto      target   = std::get<GraphBufferHandle>(builder.Import(readback, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
    FrameCustomPass custom{"custom compute"};
    custom.encoder = callback;
    custom.buffers.push_back({0, 0, {declared, 0, 64}, 0, FrameAccessMode::WRITE, FrameBufferBindingKind::STORAGE});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Custom(std::move(custom))));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy(BufferCopyPass{declared, target, 0, 0, 64})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export(GraphBufferExport{target, 0, 64})));
    Execute(std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()));
    auto *values = static_cast<const uint32_t *>(static_cast<const void *>(readback->Map()));
    ASSERT_TRUE(values);
    for (uint32_t i = 0; i < 4; ++i)
        EXPECT_EQ(values[i], i + 7);
    readback->UnMap();
}

TEST_F(D3D12FrameCompilerTest, ComputeGpuUniformsUseConstantStateAndValidRootParameters)
{
    const char *source   = "cbuffer First : register(b1) { uint first; };\n"
                           "cbuffer Second : register(b3) { uint second; };\n"
                           "RWStructuredBuffer<uint> values : register(u0, space1);\n"
                           "[numthreads(1,1,1)] void CSMain(uint3 id:SV_DispatchThreadID){ values[id.x]=first+10*second+id.x; }";
    const auto  function = CompileFunction(source, ShaderStageFlagBit::CS, "CSMain", "cs_5_1");
    ASSERT_TRUE(function);
    for (const bool dynamic : {false, true}) {
        const auto         reflection = ComputeUniformReflection(dynamic);
        Shader::Descriptor descriptor{};
        descriptor.cs         = function.Get();
        descriptor.reflection = &reflection;
        auto shader           = ShaderPtr(device->CreateShader(descriptor));
        ASSERT_TRUE(shader);
        auto [graph, readback] = ComputeUniformGraph(*device, std::move(shader), dynamic);
        ASSERT_TRUE(graph);
        Execute(std::move(graph), [&](const D3D12CompiledFrame &plan) {
            const auto &task    = plan.encoder->tasks[1];
            const auto *compute = std::get_if<D3D12ComputeStep>(&task.operation);
            ASSERT_TRUE(compute);
            EXPECT_EQ(compute->tables.size(), dynamic ? 1u : 2u);
            EXPECT_EQ(compute->uniforms.size(), dynamic ? 2u : 0u);
            for (const auto &table : compute->tables)
                EXPECT_NE(table.root, UINT32_MAX);
            EXPECT_TRUE(std::any_of(task.before.begin(), task.before.end(), [](const auto &barrier) {
                return barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION &&
                       barrier.Transition.StateAfter == D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            }));
        });
        const auto *values = reinterpret_cast<const uint32_t *>(readback->Map());
        ASSERT_TRUE(values);
        for (uint32_t i = 0; i < 4; ++i)
            EXPECT_EQ(values[i], 81u + i);
        readback->UnMap();
    }
}

TEST_F(D3D12FrameCompilerTest, ConsecutiveComputeUavAccessesSynchronizeBuffersAndImages)
{
    FrameGraphBuilder builder;
    const auto storage = std::get<GraphBufferHandle>(builder.Import(StorageBufferOf(), CopyBufferState::COMMON, CopyBufferState::COMMON, false));
    const auto image   = std::get<GraphImageHandle>(builder.Import(StorageImageOf()));
    for (const bool accumulate : {false, true}) {
        FrameComputePass pass;
        pass.shader  = ComputeShaderOf(accumulate);
        pass.groupsX = 4;
        pass.buffers.push_back({0, 0, {storage, 0, 16}, 0, accumulate ? FrameAccessMode::READ_WRITE : FrameAccessMode::WRITE});
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Compute(std::move(pass))));
        FrameComputePass fill;
        fill.shader  = ComputeImageShaderOf();
        fill.groupsX = fill.groupsY = 8;
        fill.images.push_back({0, 0, image, {}, accumulate ? FrameAccessMode::READ_WRITE : FrameAccessMode::WRITE});
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Compute(std::move(fill))));
    }
    auto       readback = BufferOf(false);
    const auto target   = std::get<GraphBufferHandle>(builder.Import(readback, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({storage, target, 0, 0, 16})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({target, 0, 16})));
    auto imageReadback = Readback(builder, image);
    Execute(std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()), [](const D3D12CompiledFrame &plan) {
        for (const auto index : {2u, 3u}) {
            const auto &barriers = plan.encoder->tasks[index].before;
            EXPECT_TRUE(std::any_of(barriers.begin(), barriers.end(),
                                    [](const auto &barrier) { return barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_UAV && barrier.UAV.pResource; }));
        }
    });
    const auto *values = reinterpret_cast<const uint32_t *>(readback->Map());
    ASSERT_TRUE(values);
    for (uint32_t i = 0; i < 4; ++i)
        EXPECT_EQ(values[i], 107u + i);
    readback->UnMap();
    const auto *pixels = imageReadback->Map();
    ASSERT_TRUE(pixels);
    EXPECT_EQ(pixels[1], 255u);
    imageReadback->UnMap();
}

TEST_F(D3D12FrameCompilerTest, ComputeRasterInterleavingIsStableAcrossWorkerCounts)
{
    struct Result {
        std::array<uint32_t, 4> values{};
        std::array<uint8_t, 4>  pixel{};
    };
    auto run = [&](uint32_t workers) {
        ResetContext(workers);
        FrameGraphBuilder builder;
        const auto        image = std::get<GraphImageHandle>(builder.Import(ImageOf()));
        FrameRasterPass   raster;
        raster.color = image;
        raster.draws.push_back({ShaderOf(), {}, {3, 1, 0, 0}, {}});
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(raster))));
        auto       storage = StorageBufferOf();
        const auto source =
            std::get<GraphBufferHandle>(builder.Import(FrameGraphBuffer{storage, 64, CopyBufferState::COMMON, CopyBufferState::COMMON, false}));
        FrameComputePass compute;
        compute.shader  = ComputeShaderOf();
        compute.groupsX = 4;
        compute.buffers.push_back({0, 0, {source, 0, 64}, 0, FrameAccessMode::WRITE});
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Compute(std::move(compute))));
        auto       imageReadback = Readback(builder, image);
        auto       host          = BufferOf(false);
        const auto target        = std::get<GraphBufferHandle>(builder.Import(host, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy(BufferCopyPass{source, target, 0, 0, 64})));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export(GraphBufferExport{target, 0, 64})));
        Execute(std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()));
        Result result;
        auto  *values = static_cast<const uint32_t *>(static_cast<const void *>(host->Map()));
        EXPECT_TRUE(values);
        if (values)
            std::copy_n(values, 4, result.values.begin());
        host->UnMap();
        auto *pixels = imageReadback->Map();
        EXPECT_TRUE(pixels);
        if (pixels)
            std::copy_n(pixels, 4, result.pixel.begin());
        imageReadback->UnMap();
        return result;
    };
    const auto serial   = run(1);
    const auto parallel = run(2);
    EXPECT_EQ(serial.values, parallel.values);
    EXPECT_EQ(serial.pixel, parallel.pixel);
    EXPECT_EQ(serial.values[0], 7u);
    EXPECT_EQ(serial.pixel[0], 255u);
    EXPECT_EQ(serial.pixel[1], 0u);
    EXPECT_EQ(serial.pixel[2], 0u);
}

TEST_F(D3D12FrameCompilerTest, SerialAndParallelProduceSameOrderedPixels)
{
    for (uint32_t workers : {1u, 2u}) {
        ResetContext(workers);
        for (uint32_t round = 0; round < 3; ++round) {
            auto [graph, buffer] = Graph();
            Execute(std::move(graph));
            auto *pixels = buffer->Map();
            ASSERT_TRUE(pixels);
            for (uint32_t y = 0; y < 8; ++y)
                for (uint32_t x = 0; x < 8; ++x) {
                    const auto i = (y * 8 + x) * 4;
                    EXPECT_EQ(pixels[i], x < 4 ? 0 : 255);
                    EXPECT_EQ(pixels[i + 1], x < 4 ? 255 : 0);
                    EXPECT_EQ(pixels[i + 2], 0);
                    EXPECT_EQ(pixels[i + 3], 255);
                }
            buffer->UnMap();
        }
    }
}

TEST_F(D3D12FrameCompilerTest, TightRowsUploadReadbackAndIntermediateLoad)
{
    FrameGraphBuilder builder;
    const auto        image  = std::get<GraphImageHandle>(builder.Import(ImageOf()));
    auto              upload = BufferOf(true, 260);
    auto             *pixels = upload->Map();
    ASSERT_TRUE(pixels);
    for (uint32_t i = 0; i < 256; ++i)
        pixels[i + 4] = static_cast<uint8_t>(i);
    upload->UnMap();
    const auto      source = std::get<GraphBufferHandle>(builder.Import(upload, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true));
    BufferImageCopy region{};
    region.bufferOffset    = 4;
    region.subRange.layers = 1;
    region.imageExtent     = {8, 8, 1};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(
        builder.Copy(BufferImageCopyPass{source, image, region, ImageCopyDirection::BUFFER_TO_IMAGE})));
    auto            before = Readback(builder, image);
    FrameRasterPass pass;
    pass.color = image;
    pass.load  = LoadOp::LOAD;
    pass.draws.push_back({ShaderOf(), {}, {3, 1, 0, 0}, {{4, 0}, {4, 8}}});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(pass))));
    auto after = Readback(builder, image);
    Execute(std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()));
    pixels = before->Map();
    ASSERT_TRUE(pixels);
    for (uint32_t i = 0; i < 256; ++i)
        EXPECT_EQ(pixels[i], static_cast<uint8_t>(i));
    before->UnMap();
    pixels = after->Map();
    ASSERT_TRUE(pixels);
    for (uint32_t i = 0; i < 256; ++i) {
        const uint8_t red[4] = {255, 0, 0, 255};
        EXPECT_EQ(pixels[i], (i / 4) % 8 < 4 ? static_cast<uint8_t>(i) : red[i % 4]);
    }
    after->UnMap();
}

TEST_F(D3D12FrameCompilerTest, ActualWorkersOverlapAndListsSubmitInPlanOrder)
{
    DxProbe probe(*device);
    probe.gateBegins = true;
    probe.delayFirst = true;
    Finally release{[&] {
        probe.Release();
        context.reset();
    }};
    auto [graph, buffer] = Graph();
    Execute(std::move(graph));
    EXPECT_GE(probe.maximum, 2u);
    EXPECT_EQ(probe.threads.size(), 2u);
    EXPECT_FALSE(probe.threads.contains(probe.coordinator));
    EXPECT_NE(probe.finishOrder.front(), 0u);
    EXPECT_EQ(probe.order, (std::vector<uint32_t>{0, 32, 64, 65, 66, 67, 68, 69, UINT32_MAX}));
    EXPECT_EQ(probe.submits, 1u);
}

TEST_F(D3D12FrameCompilerTest, LeaseBudgetsRollbackFailedCompilationAndReuseOnlyAfterRetirement)
{
    DeviceFrameContextInitInfo info{1, 2, 101, 32};
    info.allocations.renderTargets = 1;
    info.allocations.scratchBytes  = 2048;
    context.reset(device->CreateFrameContext(info));
    ASSERT_TRUE(context);
    DxProbe probe(*device);
    auto    lease = Begin();
    EXPECT_EQ(std::get<FrameError>(context->Compile(lease, Graph().first)).code, FrameErrorCode::OUT_OF_MEMORY);
    FrameGraphBuilder builder;
    auto              image = std::get<GraphImageHandle>(builder.Import(ImageOf()));
    FrameRasterPass   pass;
    pass.color = image;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(pass))));
    auto firstReadback  = Readback(builder, image);
    auto secondReadback = Readback(builder, image);
    auto twoCopies      = std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
    EXPECT_EQ(std::get<FrameError>(context->Compile(lease, twoCopies)).code, FrameErrorCode::OUT_OF_MEMORY);
    EXPECT_EQ(probe.createdLists, 0u);
    EXPECT_EQ(probe.submits, 0u);
    ID3D12DescriptorHeap *previousHeap    = nullptr;
    Buffer               *previousScratch = nullptr;
    for (uint32_t round = 0; round < 2; ++round) {
        auto [graph, buffer] = Graph(false);
        auto compiled        = context->Compile(lease, std::move(graph));
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
        auto        plan    = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
        const auto &encoder = *static_cast<D3D12CompiledFrame *>(plan.get())->encoder;
        const auto &copy    = std::get<D3D12ImageCopyStep>(encoder.tasks.back().operation);
        EXPECT_EQ(copy.footprint.Offset, 0u);
        if (round) {
            EXPECT_EQ(encoder.renderTargets.Get(), previousHeap);
            EXPECT_EQ(copy.scratch.Get(), previousScratch);
        }
        previousHeap    = encoder.renderTargets.Get();
        previousScratch = copy.scratch.Get();
        auto dispatched = context->Dispatch(plan);
        ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
        auto collected = Collect(std::get<EncodingTicket>(dispatched));
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
        auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
        EXPECT_TRUE(std::holds_alternative<FrameBusy>(context->TryBeginSlot()));
        Complete();
        const auto *pixels = buffer->Map();
        ASSERT_TRUE(pixels);
        for (uint32_t i = 0; i < 64; ++i) {
            EXPECT_EQ(pixels[i * 4], 0u);
            EXPECT_EQ(pixels[i * 4 + 1], 255u);
        }
        buffer->UnMap();
        lease = Begin();
    }
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_F(D3D12FrameCompilerTest, TwoInflightSlotsKeepTheirAllocatorsAndResources)
{
    DxProbe             probe(*device);
    ComPtr<ID3D12Fence> gate;
    ASSERT_TRUE(SUCCEEDED(device->GetNativeHandle()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gate))));
    auto *queue = static_cast<D3D12Queue *>(device->GetQueue(QueueType::GRAPHICS))->GetNativeHandle();
    ASSERT_TRUE(SUCCEEDED(queue->Wait(gate.Get(), 1)));
    Finally                                  release{[&] {
        gate->Signal(1);
        context.reset();
    }};
    std::array<std::weak_ptr<const void>, 2> weak;
    for (uint32_t i = 0; i < 2; ++i) {
        auto [graph, buffer] = Graph();
        weak[i]              = TrackGraphGpuPayload(graph);
        auto collected       = Collect(Dispatch(std::move(graph)));
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
        auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
        for (const auto &[allocator, owner] : probe.allocators)
            probe.protectedAllocators.insert(allocator);
    }
    EXPECT_TRUE(std::holds_alternative<FrameBusy>(context->TryBeginSlot()));
    for (const auto &entry : weak)
        EXPECT_FALSE(entry.expired());
    ASSERT_TRUE(SUCCEEDED(gate->Signal(1)));
    Complete(2);
    for (const auto &entry : weak)
        EXPECT_TRUE(entry.expired());
    probe.protectedAllocators.clear();
    auto [graph, buffer] = Graph();
    Execute(std::move(graph));
    EXPECT_GT(probe.resets, 0u);
}

TEST_F(D3D12FrameCompilerTest, RecordingBudgetFailsBeforeAnyWorkerStarts)
{
    ResetContext(2, 1, 2);
    DxProbe probe(*device);
    auto [graph, buffer] = Graph();
    const auto ticket    = Dispatch(std::move(graph));
    auto       result    = Collect(ticket);
    ASSERT_TRUE(std::holds_alternative<EncodingFailed>(result));
    EXPECT_EQ(std::get<EncodingFailed>(result).error.code, FrameErrorCode::OUT_OF_MEMORY);
    EXPECT_EQ(probe.createdLists, 0u);
    EXPECT_EQ(probe.submits, 0u);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(ticket.frame.lease)));
}

TEST_F(D3D12FrameCompilerTest, NativeEncodingFaultsDrainAndRetryIncludingPoisonedLists)
{
    for (auto fault :
         {DxProbe::Fault::ALLOCATOR, DxProbe::Fault::LIST, DxProbe::Fault::RESET_ALLOCATOR, DxProbe::Fault::BEGIN, DxProbe::Fault::END}) {
        SCOPED_TRACE(static_cast<int>(fault));
        ResetContext(2, 1);
        if (fault == DxProbe::Fault::RESET_ALLOCATOR) {
            auto [g, b] = Graph();
            Execute(std::move(g));
        }
        DxProbe probe(*device);
        probe.fault = fault;
        Finally cleanup{[&] { context.reset(); }};
        auto [graph, buffer]                               = Graph();
        const std::weak_ptr<const FrozenFrameGraph> weak   = graph;
        const auto                                  ticket = Dispatch(std::move(graph));
        auto                                        result = Collect(ticket);
        ASSERT_TRUE(std::holds_alternative<EncodingFailed>(result));
        EXPECT_EQ(std::get<EncodingFailed>(result).error.cause.nativeCode, E_OUTOFMEMORY);
        EXPECT_EQ(probe.concurrent, 0u);
        EXPECT_EQ(probe.submits, 0u);
        EXPECT_TRUE(weak.expired());
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryCollect(ticket)));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(ticket.frame.lease)));
        probe.fault          = DxProbe::Fault::NONE;
        auto [retry, output] = Graph();
        Execute(std::move(retry));
        EXPECT_EQ(probe.submits, 1u);
    }
}

TEST_F(D3D12FrameCompilerTest, DeviceLossDominatesEarlierOomAndCancellation)
{
    ResetContext(2, 1);
    DxProbe probe(*device);
    probe.gateBegins  = true;
    probe.holdEnd     = true;
    probe.endFailures = {{0, E_OUTOFMEMORY}, {32, DXGI_ERROR_DEVICE_REMOVED}};
    Finally release{[&] {
        probe.Release();
        context.reset();
    }};
    auto [graph, buffer]                               = Graph();
    const std::weak_ptr<const FrozenFrameGraph> weak   = graph;
    const auto                                  ticket = Dispatch(std::move(graph));
    {
        std::unique_lock lock(probe.mutex);
        ASSERT_TRUE(probe.cv.wait_for(lock, 5s, [&] { return probe.held && probe.finished; }));
    }
    EXPECT_TRUE(std::holds_alternative<FramePending>(context->TryCollect(ticket)));
    EXPECT_FALSE(weak.expired());
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->Cancel(ticket)));
    probe.Release();
    auto result = Collect(ticket);
    ASSERT_TRUE(std::holds_alternative<EncodingFailed>(result));
    EXPECT_EQ(std::get<EncodingFailed>(result).error.cause.code, GpuErrorCode::DEVICE_LOST);
    EXPECT_EQ(std::get<EncodingFailed>(result).error.cause.nativeCode, DXGI_ERROR_DEVICE_REMOVED);
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(probe.submits, 0u);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(ticket.frame.lease)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryBeginSlot()));
}

TEST_F(D3D12FrameCompilerTest, WrongTicketsAndLateCancellationDoNotConsumeAnotherSession)
{
    ResetContext(2, 1);
    DxProbe probe(*device);
    probe.gateBegins = true;
    probe.holdEnd    = true;
    Finally release{[&] {
        probe.Release();
        context.reset();
    }};
    auto [graph, buffer] = Graph();
    const auto ticket    = Dispatch(std::move(graph));
    for (uint32_t field = 0; field < 8; ++field) {
        auto wrong = ticket;
        if (field == 0)
            wrong.frame.lease.context.backend = API::VULKAN;
        if (field == 1)
            ++wrong.frame.lease.context.deviceEpoch;
        if (field == 2)
            ++wrong.frame.lease.context.contextId;
        if (field == 3)
            ++wrong.frame.lease.slot;
        if (field == 4)
            ++wrong.frame.lease.generation;
        if (field == 5)
            ++wrong.frame.graphId;
        if (field == 6)
            ++wrong.frame.planId;
        if (field == 7)
            ++wrong.sessionId;
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryCollect(wrong)));
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->Cancel(wrong)));
    }
    probe.Release();
    {
        std::unique_lock lock(probe.mutex);
        ASSERT_TRUE(probe.cv.wait_for(lock, 5s, [&] { return probe.finished == 9; }));
    }
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->Cancel(ticket)));
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->Cancel(ticket)));
    EXPECT_TRUE(std::holds_alternative<EncodingCancelled>(Collect(ticket)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryCollect(ticket)));
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(ticket.frame.lease)));
    auto [next, output]   = Graph();
    const auto nextTicket = Dispatch(std::move(next));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->Cancel(ticket)));
    auto result = Collect(nextTicket);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(result));
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(result));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->Cancel(nextTicket)));
    EXPECT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    EXPECT_TRUE(std::holds_alternative<SubmissionRejected>(context->Submit(recorded)));
    Complete();
}

TEST_F(D3D12FrameCompilerTest, UncertainSubmissionAndQueryFailureQuarantineUntilTeardown)
{
    for (bool uncertain : {false, true}) {
        ResetContext(2, 1);
        DxProbe probe(*device);
        Finally cleanup{[&] {
            probe.queryOverride = false;
            context.reset();
        }};
        auto [graph, buffer]                      = Graph();
        const std::weak_ptr<const void> weak      = TrackGraphGpuPayload(graph);
        const auto                      ticket    = Dispatch(std::move(graph));
        auto                            collected = Collect(ticket);
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
        auto recorded    = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
        probe.failSignal = uncertain;
        auto submitted   = context->Submit(recorded);
        ASSERT_EQ(std::holds_alternative<SubmissionIndeterminate>(submitted), uncertain);
        ASSERT_EQ(std::holds_alternative<SubmissionAccepted>(submitted), !uncertain);
        EXPECT_FALSE(recorded);
        EXPECT_EQ(probe.submits, 1u);
        probe.queryOverride = true;
        probe.queryValue    = 0;
        std::array<FrameCompletion, 1> output;
        EXPECT_EQ(context->PollCompleted(output), 0u);
        EXPECT_FALSE(weak.expired());
        probe.queryValue = UINT64_MAX;
        ASSERT_EQ(context->PollCompleted(output), 1u);
        EXPECT_EQ(output[0].status, FrameCompletionStatus::DEVICE_LOST);
        EXPECT_EQ(context->PollCompleted(output), 0u);
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryBeginSlot()));
        EXPECT_FALSE(weak.expired());
        probe.queryOverride = false;
        context.reset();
        EXPECT_TRUE(weak.expired());
    }
}

TEST_F(D3D12FrameCompilerTest, TeardownDrainsWorkersBeforeReleasingNativeStorage)
{
    DxProbe probe(*device);
    probe.gateBegins = true;
    probe.holdEnd    = true;
    probe.holdIdle   = true;
    std::future<void> teardown;
    Finally           release{[&] {
        probe.Release();
        {
            std::lock_guard lock(probe.mutex);
            probe.releaseIdle = true;
            probe.cv.notify_all();
        }
        if (teardown.valid())
            teardown.wait();
        context.reset();
    }};
    auto [graph, buffer]                             = Graph();
    const std::weak_ptr<const FrozenFrameGraph> weak = graph;
    Dispatch(std::move(graph));
    {
        std::unique_lock lock(probe.mutex);
        ASSERT_TRUE(probe.cv.wait_for(lock, 5s, [&] { return probe.held > 0; }));
    }
    teardown = std::async(std::launch::async, [owned = std::move(context)]() mutable { owned.reset(); });
    EXPECT_EQ(teardown.wait_for(20ms), std::future_status::timeout);
    EXPECT_FALSE(weak.expired());
    probe.Release();
    {
        std::unique_lock lock(probe.mutex);
        ASSERT_TRUE(probe.cv.wait_for(lock, 5s, [&] { return probe.idleEntered; }));
        EXPECT_EQ(probe.concurrent, 0u);
        EXPECT_EQ(probe.submits, 0u);
        probe.releaseIdle = true;
        probe.cv.notify_all();
    }
    EXPECT_EQ(teardown.wait_for(5s), std::future_status::ready);
    EXPECT_TRUE(weak.expired());
}

TEST_F(D3D12FrameCompilerTest, CustomDeviceLossSurvivesCommandCloseFailure)
{
    class Callback final : public FrameCustomPassEncoder {
    public:
        FrameOperationResult Encode(FrameCustomPassContext &) const noexcept override
        {
            return FrameError{FrameErrorCode::DEVICE_LOST, "Custom device loss"};
        }
    };
    DxProbe probe(*device);
    Finally cleanup{[&] { context.reset(); }};
    probe.fault = DxProbe::Fault::END;
    FrameGraphBuilder builder;
    const auto      buffer = std::get<GraphBufferHandle>(builder.Import(StorageBufferOf(), CopyBufferState::COMMON, CopyBufferState::COMMON, false));
    FrameCustomPass pass;
    pass.encoder = std::make_shared<Callback>();
    pass.buffers.push_back({0, 0, {buffer, 0, 16}, 0, FrameAccessMode::WRITE, FrameBufferBindingKind::STORAGE});
    pass.sideEffect = true;
    builder.Custom(std::move(pass));
    const auto lease    = Begin();
    auto       compiled = context->Compile(lease, std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    const auto collected = Collect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<EncodingFailed>(collected));
    EXPECT_EQ(std::get<EncodingFailed>(collected).error.code, FrameErrorCode::DEVICE_LOST);
    EXPECT_TRUE(probe.injected);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryBeginSlot()));
}

TEST_F(D3D12FrameCompilerTest, CompileFailuresPreserveNativeErrorsAndCloseLostDevice)
{
    for (auto fault : {DxProbe::Fault::PIPELINE, DxProbe::Fault::TARGETS}) {
        for (HRESULT error : {E_OUTOFMEMORY, DXGI_ERROR_DEVICE_REMOVED}) {
            ResetContext(2, 1);
            DxProbe probe(*device);
            probe.fault          = fault;
            probe.compileFailure = error;
            auto [graph, buffer] = Graph(false);
            const auto lease     = Begin();
            auto       result    = context->Compile(lease, std::move(graph));
            ASSERT_TRUE(std::holds_alternative<FrameError>(result));
            EXPECT_EQ(std::get<FrameError>(result).cause.nativeCode, error);
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
            auto next = context->TryBeginSlot();
            if (error == DXGI_ERROR_DEVICE_REMOVED)
                EXPECT_TRUE(std::holds_alternative<FrameError>(next));
            else {
                ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(next));
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(std::get<DeviceFrameLease>(next))));
            }
            context.reset();
        }
    }
}

TEST_F(D3D12FrameCompilerTest, UnsupportedLocalReadAndForeignResourcesAreExplicit)
{
    FrameGraphBuilder builder;
    auto              input = ImageOf();
    input.initialized       = true;
    input.initial           = ImageLayout::COLOR_ATTACHMENT;
    const auto      image   = std::get<GraphImageHandle>(builder.Import(std::move(input)));
    FrameRasterPass pass;
    pass.color          = image;
    pass.load           = LoadOp::LOAD;
    pass.colorLocalRead = true;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(pass))));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export(image)));
    const auto lease  = Begin();
    auto       result = context->Compile(lease, std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()));
    ASSERT_TRUE(std::holds_alternative<FrameError>(result));
    EXPECT_EQ(std::get<FrameError>(result).code, FrameErrorCode::UNSUPPORTED);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    auto [graph, output] = Graph(false);
    auto data            = graph->GetData();
    data.images[0].image = ImagePtr(new Image());
    const auto next      = Begin();
    result               = context->Compile(next, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
    ASSERT_TRUE(std::holds_alternative<FrameError>(result));
    EXPECT_EQ(std::get<FrameError>(result).code, FrameErrorCode::INVALID_ARGUMENT);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(next)));
}

TEST_F(D3D12FrameCompilerTest, WrongContextRejectsWithoutConsumingParallelRecording)
{
    DxProbe probe(*device);
    auto [graph, buffer]                                  = Graph();
    const std::weak_ptr<const FrozenFrameGraph> weak      = graph;
    const auto                                  ticket    = Dispatch(std::move(graph));
    auto                                        collected = Collect(ticket);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto                                recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    const auto                         *original = recorded.get();
    std::unique_ptr<DeviceFrameContext> other(device->CreateFrameContext({1, 1, 101, 32}));
    ASSERT_TRUE(other);
    EXPECT_TRUE(std::holds_alternative<SubmissionRejected>(other->Submit(recorded)));
    EXPECT_EQ(recorded.get(), original);
    EXPECT_FALSE(weak.expired());
    EXPECT_EQ(probe.submits, 0u);
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->DiscardUnsubmitted(ticket.frame.lease)));
    const auto result = context->Submit(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(result));
    EXPECT_EQ(std::get<SubmissionAccepted>(result).token.serial, 1u);
    EXPECT_EQ(probe.submits, 1u);
    EXPECT_EQ(probe.begun, 9u);
    Complete();
    EXPECT_TRUE(weak.expired());
}

TEST_F(D3D12FrameCompilerTest, DeadRasterIsCulledBeforeNativePipelineCreation)
{
    auto [graph, buffer]      = Graph(false);
    auto data                 = graph->GetData();
    auto dead                 = std::get<FrameRasterPass>(data.passes.front());
    dead.draws.front().shader = ShaderPtr(new Shader());
    data.passes.insert(data.passes.begin(), dead);
    graph               = std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data));
    const auto lease    = Begin();
    auto       compiled = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto        plan   = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    const auto *native = static_cast<const D3D12CompiledFrame *>(plan.get());
    ASSERT_EQ(native->encoder->tasks.size(), 2u);
    EXPECT_EQ(native->encoder->tasks[0].logicalPass, 1u);
    EXPECT_EQ(native->encoder->tasks[1].logicalPass, 2u);
    const auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto result = Collect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(result));
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(result));
    EXPECT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    Complete();
}

TEST_F(D3D12FrameCompilerTest, ExportWithoutPassStillTransitionsImportedImage)
{
    auto              image = ImageOf();
    FrameGraphBuilder first;
    const auto        target = std::get<GraphImageHandle>(first.Import(image));
    FrameRasterPass   clear;
    clear.color = target;
    clear.clear = {0.f, 0.f, 1.f, 1.f};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(first.Raster(std::move(clear))));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(first.Export(target)));
    Execute(std::get<std::shared_ptr<const FrozenFrameGraph>>(first.Freeze()));

    image.initialized = true;
    image.initial     = ImageLayout::TRANSFER_SRC;
    image.final       = ImageLayout::COLOR_ATTACHMENT;
    FrameGraphBuilder boundary;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(boundary.Export(std::get<GraphImageHandle>(boundary.Import(image)))));
    auto compiled = context->Compile(Begin(), std::get<std::shared_ptr<const FrozenFrameGraph>>(boundary.Freeze()));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto        plan   = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    const auto *native = static_cast<const D3D12CompiledFrame *>(plan.get());
    ASSERT_EQ(native->encoder->tasks.size(), 1u);
    ASSERT_EQ(native->encoder->tasks[0].after.size(), 1u);
    EXPECT_EQ(native->encoder->tasks[0].after[0].Transition.StateBefore, D3D12_RESOURCE_STATE_COPY_SOURCE);
    EXPECT_EQ(native->encoder->tasks[0].after[0].Transition.StateAfter, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = Collect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recording = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    Complete();

    image.initial = ImageLayout::COLOR_ATTACHMENT;
    image.final   = ImageLayout::TRANSFER_SRC;
    FrameGraphBuilder last;
    auto              buffer = Readback(last, std::get<GraphImageHandle>(last.Import(image)));
    Execute(std::get<std::shared_ptr<const FrozenFrameGraph>>(last.Freeze()));
    auto *pixels = buffer->Map();
    ASSERT_TRUE(pixels);
    for (uint32_t i = 0; i < 256; ++i)
        EXPECT_EQ(pixels[i], i % 4 >= 2 ? 255 : 0);
    buffer->UnMap();
}
