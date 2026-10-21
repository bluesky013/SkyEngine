//
// Created on 2026/10/17.
//

#include "DepthSamplingFixture.h"
#include "HostSceneFixture.h"
#include "OpaquePass.h"
#include "SceneBridgeFixture.h"
#include "frame/ViewportMapping.h"
#include "graph/FrameFeatureRegistry.h"
#include <D3D12Device.h>
#include <VulkanDevice.h>
#include <array>
#include <atomic>
#include <aurora/adaptor/renderer/RendererHost.h>
#include <aurora/adaptor/renderer/UiRenderFeature.h>
#include <aurora/pipeline/SceneMaterial.h>
#include <aurora/pipeline/SceneRenderFeature.h>
#include <aurora/pipeline/TextureToScreenFeature.h>
#include <aurora/renderer/RendererProvider.h>
#include <aurora/rhi/Instance.h>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <chrono>
#include <cstring>
#include <d3d12sdklayers.h>
#include <editor/render/UiSnapshotProducer.h>
#include <gtest/gtest.h>
#include <limits>
#include <thread>
#include <ui/UIPaintContext.h>
#include <windows.h>

using namespace sky;
using namespace sky::aurora;
using namespace std::chrono_literals;

namespace {
    class HeaderColorFeature final : public IFrameGraphFeature {
    public:
        HeaderColorFeature(RenderMaterialData code, bool shortRange) : code(std::move(code)), shortRange(shortRange)
        {
        }
        Name GetName() const override
        {
            return Name("header.color");
        }
        FrameOperationResult BuildFrame(FrameGraphBuildContext &context, const FrameExtensionPayload &payload) override
        {
            if (payload.outputs.size() != 1 || payload.bytes.size() != sizeof(shared::OpaquePassParams))
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid color pass payload"};
            const auto output = context.GetOutput(payload.outputs.front());
            if (!output)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Missing color pass output"};
            if (!shader) {
                auto created = context.CreateShader(code);
                if (const auto *error = std::get_if<FrameError>(&created))
                    return *error;
                shader = std::get<ShaderPtr>(created);
            }
            const auto &layout = shared::GetOpaquePassParamsLayout();
            auto        written =
                context.WriteUniform(layout.set, layout.binding, std::as_bytes(std::span{payload.bytes}), ShaderResourceType::UNIFORM_BUFFER_DYNAMIC);
            if (const auto *error = std::get_if<FrameError>(&written))
                return *error;
            RasterDraw draw;
            draw.shader              = shader;
            draw.command.vertexCount = 3;
            draw.scissor.extent      = output->output.extent;
            draw.uniforms.push_back(std::get<RasterUniformBinding>(written));
            if (shortRange)
                draw.uniforms.front().range.size -= 4;
            FrameRasterPass pass;
            pass.color = output->color;
            pass.draws.push_back(std::move(draw));
            return context.Fullscreen(std::move(pass));
        }
        void OnRetired() override
        {
            shader = {};
        }

    private:
        RenderMaterialData code;
        bool               shortRange;
        ShaderPtr          shader;
    };

    std::shared_ptr<IFrameGraphFeature> HeaderColor(RendererInfo info, bool shortRange = false)
    {
        auto code = CompileHostRaster(info, R"(
            #include "OpaquePass.h"
            float4 VSMain(uint id : SV_VertexID) : SV_Position {
                float2 p = float2((id << 1) & 2, id & 2);
                return float4(p * 2 - 1, 0, 1);
            }
            float4 FSMain() : SV_Target { return gOpaquePass.Misc; }
        )");
        for (auto &resource : code.reflection.resources)
            resource.type = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
        return std::make_shared<HeaderColorFeature>(std::move(code), shortRange);
    }

    FrameExtensionPayload HeaderColorPayload(ViewportId output, Vector4 color)
    {
        FrameExtensionPayload payload;
        payload.feature = Name("header.color");
        payload.outputs = {output};
        const shared::OpaquePassParams params{color};
        payload.bytes.resize(sizeof(params));
        std::memcpy(payload.bytes.data(), &params, sizeof(params));
        return payload;
    }

    std::shared_ptr<IFrameGraphFeature> SampledColor(RendererInfo info)
    {
        return CreateTextureToScreenFeature(Name("sampled.color"), CompileHostRaster(info, R"(
            #include "TextureToScreenPass.h"
            struct Vertex { float4 position : SV_Position; float2 uv : TEXCOORD0; };
            Vertex VSMain(uint id : SV_VertexID) {
                float2 p = float2((id << 1) & 2, id & 2);
                Vertex result;
                result.position = float4(p * 2 - 1, 0, 1);
                result.uv = p;
                return result;
            }
            float4 FSMain(Vertex vertex) : SV_Target {
                return gSourceTexture.Sample(gSourceSampler, vertex.uv) * gTextureToScreen.Misc;
            }
        )"));
    }
    struct FeatureCounters {
        uint32_t builds = 0, retired = 0;
        bool     fail = false;
    };
    class CountingFeature final : public IFrameGraphFeature {
    public:
        explicit CountingFeature(std::shared_ptr<FeatureCounters> counters) : counters(std::move(counters))
        {
        }
        Name GetName() const override
        {
            return Name("count");
        }
        FrameOperationResult BuildFrame(FrameGraphBuildContext &, const FrameExtensionPayload &) override
        {
            ++counters->builds;
            if (counters->fail)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Injected feature failure"};
            return FrameOperationComplete{};
        }
        void OnRetired() override
        {
            ++counters->retired;
        }

    private:
        std::shared_ptr<FeatureCounters> counters;
    };
    struct EncodeFault {
        explicit EncodeFault(Device &device, bool fatal = false) : owner(&device), fatal(fatal)
        {
            active = this;
            if (device.GetAPI() == API::VULKAN) {
                vk                     = &const_cast<VulkanDeviceFunctions &>(static_cast<VulkanDevice &>(device).GetDeviceFn());
                savedVk                = vk->vkEndCommandBuffer;
                vk->vkEndCommandBuffer = [](VkCommandBuffer command) -> VkResult {
                    return !active->triggered.exchange(true) ? (active->fatal ? VK_ERROR_DEVICE_LOST : VK_ERROR_OUT_OF_HOST_MEMORY)
                                                             : active->savedVk(command);
                };
            } else {
                dx            = &const_cast<D3D12Functions &>(static_cast<D3D12Device &>(device).GetFunctions());
                savedDx       = dx->closeList;
                dx->closeList = [](ID3D12GraphicsCommandList *command) -> HRESULT {
                    return !active->triggered.exchange(true) ? (active->fatal ? DXGI_ERROR_DEVICE_REMOVED : E_OUTOFMEMORY) : active->savedDx(command);
                };
            }
        }
        ~EncodeFault()
        {
            if (Instance::Get()->GetDevice() == owner) {
                if (vk)
                    vk->vkEndCommandBuffer = savedVk;
                if (dx)
                    dx->closeList = savedDx;
            }
            active = nullptr;
        }
        static inline EncodeFault *active = nullptr;
        Device                    *owner;
        bool                       fatal;
        std::atomic_bool           triggered            = false;
        VulkanDeviceFunctions     *vk                   = nullptr;
        PFN_vkEndCommandBuffer     savedVk              = nullptr;
        D3D12Functions            *dx                   = nullptr;
        HRESULT (*savedDx)(ID3D12GraphicsCommandList *) = nullptr;
    };
    constexpr SceneId    scene{0, 1};
    constexpr ViewportId viewport{0, 1};
    constexpr ViewId     view{0, 1};
    class RendererRuntimeTest : public testing::TestWithParam<uint32_t> {
    protected:
        void SetUp() override
        {
            info.backend          = GetParam() == 2 ? API::DX12 : API::VULKAN;
            info.vulkanMode       = GetParam() == 1 ? RendererVulkanMode::RENDER_PASS : RendererVulkanMode::DYNAMIC_RENDERING;
            info.prepareWorkers   = 3;
            info.encodeWorkers    = 3;
            info.enableValidation = true;
            provider              = CreateRendererProvider();
            Create();
        }
        void Create()
        {
            auto created = provider->Create(info);
            if (const auto *error = std::get_if<RendererError>(&created))
                FAIL() << error->message;
            renderer = std::move(std::get<std::unique_ptr<IRenderer>>(created));
            ASSERT_TRUE(renderer);
            EXPECT_EQ(renderer->GetInfo().backend, info.backend);
            EXPECT_NE(renderer->GetInfo().deviceEpoch, 0u);
            if (info.backend == API::DX12) {
                ASSERT_TRUE(
                    SUCCEEDED(static_cast<D3D12Device *>(Instance::Get()->GetDevice())->GetNativeHandle()->QueryInterface(IID_PPV_ARGS(&debug))));
                debug->ClearStoredMessages();
            }
        }
        void TearDown() override
        {
            Stop();
            provider.reset();
            Instance::Destroy();
        }
        void Pump()
        {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            std::array<RendererEvent, 8> batch;
            const auto                   count = renderer->PollEvents(batch);
            for (size_t i = 0; i < count; ++i)
                events.push_back(std::move(batch[i]));
        }
        template <typename Predicate>
        bool Until(Predicate predicate)
        {
            const auto deadline = std::chrono::steady_clock::now() + 10s;
            do {
                Pump();
                if (predicate())
                    return true;
                std::this_thread::sleep_for(1ms);
            } while (std::chrono::steady_clock::now() < deadline);
            return false;
        }
        template <typename Event>
        const Event *Find() const
        {
            for (const auto &event : events)
                if (const auto *value = std::get_if<Event>(&event))
                    return value;
            return nullptr;
        }
        void Stop()
        {
            if (!renderer)
                return;
            renderer->RequestShutdown();
            EXPECT_TRUE(Until([&] { return Find<RendererStopped>(); }));
            EXPECT_EQ(renderer->GetState(), RendererState::STOPPED);
            renderer.reset();
            if (debug) {
                for (UINT64 i = 0; i < debug->GetNumStoredMessages(); ++i) {
                    SIZE_T size = 0;
                    debug->GetMessage(i, nullptr, &size);
                    std::vector<char> bytes(size);
                    auto             *message = reinterpret_cast<D3D12_MESSAGE *>(bytes.data());
                    if (SUCCEEDED(debug->GetMessage(i, message, &size)))
                        EXPECT_GT(message->Severity, D3D12_MESSAGE_SEVERITY_ERROR) << message->pDescription;
                }
                debug.Reset();
            }
        }
        std::unique_ptr<FramePacket> Packet(std::vector<RenderCommand> commands = {})
        {
            auto packet      = std::make_unique<FramePacket>();
            packet->sequence = ++sequence;
            packet->commands = std::move(commands);
            return packet;
        }
        std::unique_ptr<FramePacket> Scene()
        {
            ViewDesc camera;
            camera.scene  = scene;
            camera.output = viewport;
            ViewportDesc output;
            output.extent = {8, 8};
            auto packet   = Packet({CreateScene{scene, {}}, CreateViewport{viewport, output}, CreateView{view, camera}});
            EXPECT_FALSE(RegisterScenePass(*packet, {63, 1}, std::make_shared<int>(1)));
            packet->readbacks.push_back(viewport);
            return packet;
        }
        void Submit(std::unique_ptr<FramePacket> packet)
        {
            EXPECT_TRUE(Until([&] { return renderer->TrySubmit(packet).status != PacketSubmitStatus::BUSY; }));
            EXPECT_FALSE(packet);
        }
        void Complete(uint64_t target)
        {
            ASSERT_TRUE(Until([&] {
                return std::any_of(events.begin(), events.end(), [&](const auto &event) {
                    const auto *completed = std::get_if<FrameCompleted>(&event);
                    const auto *failed    = std::get_if<RendererFailure>(&event);
                    return (completed && completed->sequence == target) || (failed && failed->packetSequence == target);
                });
            }));
            for (const auto &event : events)
                if (const auto *failed = std::get_if<RendererFailure>(&event))
                    ADD_FAILURE() << failed->packetSequence << ": " << failed->error.message;
        }
        RenderResourceKey Key(uint32_t id, uint64_t revision = 1) const
        {
            return {Uuid::CreateWithSeed(id), revision, 1, renderer->GetInfo().deviceEpoch};
        }
        RenderResourcePayload Mesh()
        {
            return HostMesh(renderer->GetInfo());
        }
        RenderResourcePayload Material()
        {
            return HostMaterial(renderer->GetInfo());
        }
        std::shared_ptr<IFrameGraphFeature> Ui()
        {
            RenderMaterialData code;
            code.backend       = info.backend;
            code.vertexEntry   = "VSMain";
            code.fragmentEntry = "FSMain";
            ShaderCompilerSlang compiler;
            for (const bool vertex : {true, false}) {
                ShaderCompileDesc desc;
                desc.source = std::string(RenderUiShaderSource());
                desc.entry  = vertex ? "VSMain" : "FSMain";
                desc.stage  = vertex ? ShaderStageFlagBit::VS : ShaderStageFlagBit::FS;
                desc.target = info.backend == API::DX12 ? ShaderTarget::DXIL : ShaderTarget::SPIRV;
                ShaderCompileResult compiled;
                EXPECT_TRUE(compiler.Compile(desc, compiled)) << compiled.errorInfo;
                auto &bytes = vertex ? code.vertexCode : code.fragmentCode;
                bytes.resize(compiled.data.size() * sizeof(uint32_t));
                std::memcpy(bytes.data(), compiled.data.data(), bytes.size());
            }
            ShaderResource image{"image", ShaderResourceType::SAMPLED_IMAGE, 0, 0, 1};
            image.texture = {ShaderTextureDimension::D2, ShaderScalarType::FLOAT, 4, false, false};
            ShaderResource sampler{"imageSampler", ShaderResourceType::SAMPLER, 0, 1, 1};
            sampler.sampler           = ShaderSamplerType::REGULAR;
            code.reflection.resources = {image, sampler};
            code.blend.blendEn        = true;
            code.blend.srcColor       = BlendFactor::SRC_ALPHA;
            code.blend.dstColor       = BlendFactor::ONE_MINUS_SRC_ALPHA;
            code.blend.srcAlpha       = BlendFactor::ONE;
            code.blend.dstAlpha       = BlendFactor::ONE_MINUS_SRC_ALPHA;
            auto created              = CreateUiRenderFeature(Name("ui"), std::move(code));
            EXPECT_TRUE(std::holds_alternative<std::shared_ptr<IFrameGraphFeature>>(created));
            return std::get<std::shared_ptr<IFrameGraphFeature>>(created);
        }
        FrameExtensionPayload UiSnapshot(ViewportId target, ViewOutputRef source, float alpha = 1)
        {
            RenderUiSnapshot snapshot;
            snapshot.output   = target;
            snapshot.vertices = {{0, 0, 0, 0}, {1, 0, 1, 0}, {0, 1, 0, 1}, {1, 1, 1, 1}};
            for (auto &vertex : snapshot.vertices)
                vertex.color[3] = alpha;
            snapshot.indices = {0, 1, 2, 2, 1, 3};
            snapshot.draws   = {{source, 0, 6}};
            auto encoded     = EncodeRenderUiSnapshot(Name("ui"), snapshot);
            EXPECT_TRUE(std::holds_alternative<FrameExtensionPayload>(encoded));
            return std::get<FrameExtensionPayload>(std::move(encoded));
        }
        void AddMesh(FramePacket &packet)
        {
            RenderEntityDesc entity;
            entity.mesh     = {Key(1201), 1};
            entity.material = {Key(1202), 1};
            packet.commands.push_back(CreateRenderEntity{{scene, {0, 1}}, entity});
            packet.commands.push_back(ResourceCpuReady{Mesh()});
            packet.commands.push_back(ResourceCpuReady{Material()});
        }
        const ViewportReadback *Pixels(uint64_t seq, ViewportId id) const
        {
            for (const auto &event : events)
                if (const auto *read = std::get_if<ViewportReadback>(&event); read && read->sequence == seq && read->viewport == id)
                    return read;
            return nullptr;
        }
        void Pixel(uint64_t seq, ViewportId id, uint32_t x, uint32_t y, std::array<int, 3> rgb)
        {
            const auto *read = Pixels(seq, id);
            ASSERT_TRUE(read);
            ASSERT_LT(x, read->extent.width);
            ASSERT_LT(y, read->extent.height);
            const auto offset = (y * read->extent.width + x) * 4;
            for (uint32_t i = 0; i < 3; ++i)
                EXPECT_NEAR(read->bytes[offset + i], rgb[i], 2) << "sequence=" << seq << " pixel=" << x << "," << y;
        }
        RendererInitInfo                   info;
        ComPtr<ID3D12InfoQueue>            debug;
        std::unique_ptr<IRendererProvider> provider;
        std::unique_ptr<IRenderer>         renderer;
        std::vector<RendererEvent>         events;
        uint64_t                           sequence = 0;
    };
} // namespace

TEST_P(RendererRuntimeTest, WorldBridgeProductionAssetsTransformAndRemovalReachPixels)
{
    auto *context = SerializationContext::Get();
    if (!TypeInfo<TransformComponent>::RegisteredId())
        TransformComponent::Reflect(context);
    AuroraReflection(context);
    struct CompilerScope {
        ShaderCompilerSlang compiler;
        IShaderCompiler    *previous = ShaderCompilerFactory::Get().GetCompiler();
        CompilerScope()
        {
            ShaderCompilerFactory::Get().Register(&compiler);
        }
        ~CompilerScope()
        {
            ShaderCompilerFactory::Get().Register(previous);
        }
    } compiler;
    WorldPtr world = World::CreateWorld();
    auto    *actor = world->CreateActor();
    actor->AddComponent<StaticMeshComponent>()->SetMeshUuid(ScenePrimitiveId(ScenePrimitive::CUBE));
    auto *transform = actor->GetComponent<TransformComponent>();
    transform->SetLocalTranslation({0, 0, .5f});
    auto              source = CreateSceneAssetSource(std::make_shared<ShaderFileSystem>());
    RenderSceneBridge bridge(renderer->GetInfo(), source);
    bridge.SetWorld(world.Get());
    auto              packet = Scene();
    struct SceneProbe final : IFrameGraphFeature {
        std::atomic<uint32_t> draws{0}, extracted{0};
        Name                  GetName() const override
        {
            return Name("scene.probe");
        }
        FrameOperationResult BuildFrame(FrameGraphBuildContext &, const FrameExtensionPayload &) override
        {
            return FrameOperationComplete{};
        }
        FrameOperationResult BuildView(FrameGraphBuildContext &context, const PreparedViewData &view) override
        {
            draws.store(static_cast<uint32_t>(view.draws.size()));
            extracted.store(static_cast<uint32_t>(context.GetInputs().prepared->views.front().draws.size()));
            return FrameOperationComplete{};
        }
    };
    auto probe = std::make_shared<SceneProbe>();
    packet->featureCommands.push_back(RegisterRenderFeature{{62, 1}, probe->GetName(), std::make_shared<int>(1), probe});
    ASSERT_FALSE(bridge.Append(*packet, scene));
    ASSERT_TRUE(bridge.GetIssues().empty()) << bridge.GetIssues().front().message;
    ASSERT_EQ(std::count_if(packet->commands.begin(), packet->commands.end(),
                            [](const auto &command) { return std::holds_alternative<CreateRenderEntity>(command); }),
              1);
    Submit(std::move(packet));
    Complete(1);
    auto visible = Packet();
    visible->readbacks.push_back(viewport);
    ASSERT_FALSE(bridge.Append(*visible, scene));
    Submit(std::move(visible));
    Complete(2);
    EXPECT_EQ(probe->draws.load(), 1u);
    EXPECT_EQ(probe->extracted.load(), 1u);
    for (const auto &event : events) {
        if (const auto *failure = std::get_if<ResourceStateChanged>(&event); failure && failure->state == RenderResourceState::FAILED)
            ADD_FAILURE() << "Scene resource: " << failure->error.message;
        if (const auto *failure = std::get_if<ScenePrepareFailed>(&event))
            ADD_FAILURE() << "Scene prepare: " << failure->error.message;
    }
    Pixel(2, viewport, 4, 4, {166, 184, 209});
    transform->SetLocalTranslation({5, 0, .5f});
    auto moved = Packet();
    moved->readbacks.push_back(viewport);
    ASSERT_FALSE(bridge.Append(*moved, scene));
    Submit(std::move(moved));
    Complete(3);
    Pixel(3, viewport, 4, 4, {0, 0, 0});
    auto detached = world->DetachFromWorld(actor);
    auto removed  = Packet();
    removed->readbacks.push_back(viewport);
    ASSERT_FALSE(bridge.Append(*removed, scene));
    detached.reset();
    world.Reset(nullptr);
    source.reset();
    Submit(std::move(removed));
    Complete(4);
    ASSERT_TRUE(Until([&] { return Find<EntityDestroyed>() != nullptr; }));
    for (const auto &event : events)
        bridge.OnEvent(event);
    Pixel(4, viewport, 4, 4, {0, 0, 0});
}

TEST(FrameFeatureRegistryTest, DisableStopsBorrowersAndRetirementWaitsForTheFinalLease)
{
    FrameFeatureRegistry registry;
    auto                 counters = std::make_shared<FeatureCounters>();
    auto                 module   = std::make_shared<int>(1);
    std::weak_ptr<int>   unloaded = module;
    FramePacket          packet;
    packet.featureCommands.push_back(RegisterRenderFeature{{0, 1}, Name("count"), std::move(module), std::make_shared<CountingFeature>(counters)});
    ASSERT_FALSE(registry.Validate(packet));
    registry.Apply(packet);
    auto borrowers = registry.Snapshot();
    packet.featureCommands.push_back(SetRenderFeatureEnabled{{0, 1}, false});
    ASSERT_FALSE(registry.Validate(packet));
    registry.Apply(packet);
    EXPECT_FALSE(borrowers.front().CanBuild());
    EXPECT_TRUE(registry.Snapshot().empty());
    packet.featureCommands.push_back(SetRenderFeatureEnabled{{0, 1}, true});
    ASSERT_FALSE(registry.Validate(packet));
    registry.Apply(packet);
    EXPECT_FALSE(borrowers.front().CanBuild());
    EXPECT_TRUE(registry.Snapshot().front().CanBuild());
    packet.featureCommands.push_back(RetireRenderFeature{{0, 1}});
    ASSERT_FALSE(registry.Validate(packet));
    registry.Apply(packet);
    EXPECT_TRUE(registry.CollectRetired().empty());
    EXPECT_FALSE(unloaded.expired());
    EXPECT_EQ(counters->retired, 0u);
    borrowers.clear();
    EXPECT_EQ(registry.CollectRetired(), (std::vector<RenderFeatureId>{{0, 1}}));
    EXPECT_TRUE(unloaded.expired());
    EXPECT_EQ(counters->retired, 1u);
    packet.featureCommands.push_back(
        RegisterRenderFeature{{0, 1}, Name("count"), std::make_shared<int>(1), std::make_shared<CountingFeature>(counters)});
    EXPECT_TRUE(registry.Validate(packet));
    std::get<RegisterRenderFeature>(packet.featureCommands.front()).id.generation = 2;
    EXPECT_FALSE(registry.Validate(packet));
}

TEST(FrameFeatureRegistryTest, InvalidSecondCommandDoesNotPartiallyRegisterTheFirst)
{
    FrameFeatureRegistry registry;
    FramePacket          packet;
    auto                 counters = std::make_shared<FeatureCounters>();
    packet.featureCommands.push_back(
        RegisterRenderFeature{{0, 1}, Name("count"), std::make_shared<int>(1), std::make_shared<CountingFeature>(counters)});
    packet.featureCommands.push_back(SetRenderFeatureEnabled{{1, 1}, false});
    EXPECT_TRUE(registry.Validate(packet));
    EXPECT_TRUE(registry.Snapshot().empty());
    packet.featureCommands.pop_back();
    EXPECT_FALSE(registry.Validate(packet));
    registry.Apply(packet);
    registry.Shutdown();
    EXPECT_EQ(registry.CollectRetired().size(), 1u);
}

TEST_P(RendererRuntimeTest, B13FeatureBuildFailureDiscardsTheFrameThenRetires)
{
    auto counters  = std::make_shared<FeatureCounters>();
    counters->fail = true;
    auto packet    = Scene();
    packet->featureCommands.push_back(
        RegisterRenderFeature{{0, 1}, Name("count"), std::make_shared<int>(1), std::make_shared<CountingFeature>(counters)});
    packet->extensions.push_back({Name("count"), {}, {viewport}});
    Submit(std::move(packet));
    ASSERT_TRUE(Until([&] { return Find<RendererFailure>(); }));
    EXPECT_FALSE(Find<FrameCompleted>());
    events.clear();
    auto next = Packet();
    next->featureCommands.push_back(RetireRenderFeature{{0, 1}});
    next->readbacks = {viewport};
    Submit(std::move(next));
    Complete(sequence);
    EXPECT_TRUE(Until([&] { return Find<RenderFeatureRetired>(); }));
    EXPECT_EQ(counters->retired, 1u);
    EXPECT_EQ(counters->builds, 1u);
}

TEST_P(RendererRuntimeTest, B13OffscreenToUiPixelsAndStaleRevisionSkipsOldContents)
{
    const ViewportId ui{1, 1};
    auto             packet = Scene();
    AddMesh(*packet);
    ViewportDesc target;
    target.extent = {16, 8};
    packet->commands.push_back(CreateViewport{ui, target});
    packet->featureCommands.push_back(RegisterRenderFeature{{0, 1}, Name("ui"), std::make_shared<int>(1), Ui()});
    packet->extensions.push_back(UiSnapshot(ui, {viewport, 0}));
    packet->readbacks.push_back(ui);
    Submit(std::move(packet));
    Complete(sequence);
    Pixel(sequence, ui, 8, 4, {255, 0, 0});
    Pixel(sequence, ui, 0, 0, {0, 0, 0});
    auto stale = Packet({ResizeViewport{viewport, {12, 6}, 1}});
    stale->extensions.push_back(UiSnapshot(ui, {viewport, 0}));
    stale->readbacks = {ui};
    Submit(std::move(stale));
    Complete(sequence);
    Pixel(sequence, ui, 8, 4, {0, 0, 0});
    auto fresh = Packet();
    fresh->extensions.push_back(UiSnapshot(ui, {viewport, 1}));
    fresh->readbacks = {ui};
    Submit(std::move(fresh));
    Complete(sequence);
    Pixel(sequence, ui, 8, 4, {255, 0, 0});
}

TEST_P(RendererRuntimeTest, B13TwoCamerasSecondSceneAndUiLoadUseSeparateConstants)
{
    const ViewportId cameraOutput{1, 1}, secondOutput{2, 1};
    const SceneId    secondScene{1, 1};
    auto             packet = Scene();
    AddMesh(*packet);
    ViewportDesc other;
    other.extent = {16, 12};
    packet->commands.push_back(CreateViewport{cameraOutput, other});
    other.extent = {12, 16};
    packet->commands.push_back(CreateViewport{secondOutput, other});
    ViewDesc camera;
    camera.scene      = scene;
    camera.output     = cameraOutput;
    camera.view.v[12] = 1.f;
    packet->commands.push_back(CreateView{{1, 1}, camera});
    packet->commands.push_back(CreateScene{secondScene, {}});
    camera.scene  = secondScene;
    camera.output = secondOutput;
    camera.view   = RenderIdentityMatrix();
    packet->commands.push_back(CreateView{{2, 1}, camera});
    auto green     = Material();
    green.resource = Key(1203);
    const float tint[]{0, 1, 0, 1};
    std::memcpy(green.bytes.data(), tint, sizeof(tint));
    RenderEntityDesc entity;
    entity.mesh     = {Key(1201), 1};
    entity.material = {Key(1203), 1};
    packet->commands.push_back(CreateRenderEntity{{secondScene, {0, 1}}, entity});
    packet->commands.push_back(ResourceCpuReady{std::move(green)});
    packet->featureCommands.push_back(RegisterRenderFeature{{0, 1}, Name("ui"), std::make_shared<int>(1), Ui()});
    packet->extensions.push_back(UiSnapshot(secondOutput, {viewport, 0}, .5f));
    packet->readbacks = {viewport, cameraOutput, secondOutput};
    Submit(std::move(packet));
    Complete(sequence);
    Pixel(sequence, viewport, 4, 4, {255, 0, 0});
    Pixel(sequence, cameraOutput, 4, 6, {0, 0, 0});
    Pixel(sequence, cameraOutput, 12, 6, {255, 0, 0});
    Pixel(sequence, secondOutput, 6, 8, {128, 127, 0});
    auto next       = Packet({DestroyRenderEntity{{scene, {0, 1}}}});
    next->readbacks = {viewport, secondOutput};
    Submit(std::move(next));
    Complete(sequence);
    Pixel(sequence, viewport, 4, 4, {0, 0, 0});
    Pixel(sequence, secondOutput, 6, 8, {0, 255, 0});
}

TEST_P(RendererRuntimeTest, B13DisableAndRetireFeatureReleaseModuleAfterFrames)
{
    const ViewportId   ui{1, 1};
    auto               owner    = std::make_shared<int>(1);
    std::weak_ptr<int> lifetime = owner;
    auto               packet   = Scene();
    AddMesh(*packet);
    ViewportDesc target;
    target.extent = {8, 8};
    packet->commands.push_back(CreateViewport{ui, target});
    ViewDesc uiScene;
    uiScene.scene    = scene;
    uiScene.output   = ui;
    uiScene.drawTags = 0;
    packet->commands.push_back(CreateView{{1, 1}, uiScene});
    packet->featureCommands.push_back(RegisterRenderFeature{{0, 1}, Name("ui"), std::move(owner), Ui()});
    packet->extensions.push_back(UiSnapshot(ui, {viewport, 0}));
    packet->readbacks = {ui};
    Submit(std::move(packet));
    Complete(sequence);
    Pixel(sequence, ui, 4, 4, {255, 0, 0});
    EXPECT_FALSE(lifetime.expired());
    auto disable = Packet();
    disable->featureCommands.push_back(SetRenderFeatureEnabled{{0, 1}, false});
    disable->extensions.push_back(UiSnapshot(ui, {viewport, 0}));
    disable->readbacks = {ui};
    Submit(std::move(disable));
    Complete(sequence);
    Pixel(sequence, ui, 4, 4, {0, 0, 0});
    auto retire = Packet();
    retire->featureCommands.push_back(RetireRenderFeature{{0, 1}});
    Submit(std::move(retire));
    Complete(sequence);
    EXPECT_TRUE(Until([&] { return Find<RenderFeatureRetired>(); }));
    EXPECT_TRUE(lifetime.expired());
    auto replace = Packet();
    replace->featureCommands.push_back(RegisterRenderFeature{{0, 2}, Name("ui"), std::make_shared<int>(2), Ui()});
    replace->extensions.push_back(UiSnapshot(ui, {viewport, 0}));
    replace->readbacks = {ui};
    Submit(std::move(replace));
    Complete(sequence);
    Pixel(sequence, ui, 4, 4, {255, 0, 0});
}

TEST_P(RendererRuntimeTest, B13MalformedUiPayloadRejectsAtomicallyThenValidFrameContinues)
{
    auto packet = Scene();
    packet->featureCommands.push_back(RegisterRenderFeature{{0, 1}, Name("ui"), std::make_shared<int>(1), Ui()});
    auto payload = UiSnapshot({1, 1}, {viewport, 0});
    payload.bytes.pop_back();
    packet->extensions.push_back(std::move(payload));
    Submit(std::move(packet));
    ASSERT_TRUE(Until([&] { return Find<RendererFailure>(); }));
    EXPECT_EQ(Find<RendererFailure>()->error.code, RendererErrorCode::INVALID_PACKET);
    EXPECT_FALSE(Find<PacketApplied>());
    events.clear();
    auto valid = Scene();
    Submit(std::move(valid));
    Complete(sequence);
    EXPECT_TRUE(Pixels(sequence, viewport));
}

TEST_P(RendererRuntimeTest, B13QueuedCompositionFramesKeepTheirOwnRevisionAndSize)
{
    const ViewportId ui{1, 1};
    auto             initial = Scene();
    AddMesh(*initial);
    ViewportDesc desc;
    desc.extent = {8, 8};
    initial->commands.push_back(CreateViewport{ui, desc});
    initial->featureCommands.push_back(RegisterRenderFeature{{0, 1}, Name("ui"), std::make_shared<int>(1), Ui()});
    initial->extensions.push_back(UiSnapshot(ui, {viewport, 0}));
    initial->readbacks = {ui};
    Submit(std::move(initial));
    Complete(sequence);
    ASSERT_TRUE(Until([&] {
        return std::count_if(events.begin(), events.end(), [](const auto &event) {
                   const auto *state = std::get_if<ResourceStateChanged>(&event);
                   return state && state->state == RenderResourceState::RESIDENT;
               }) == 2;
    }));
    for (uint64_t revision = 1; revision <= 6; ++revision) {
        auto packet = Packet({ResizeViewport{viewport, {static_cast<uint32_t>(8 + revision), 8}, revision},
                              ResizeViewport{ui, {static_cast<uint32_t>(16 + revision), 8}, revision}});
        packet->extensions.push_back(UiSnapshot(ui, {viewport, revision}));
        packet->readbacks = {ui};
        Submit(std::move(packet));
    }
    renderer->RequestShutdown();
    ASSERT_TRUE(Until([&] { return Find<RendererStopped>(); }));
    for (uint64_t seq = 1; seq <= sequence; ++seq) {
        const auto *pixels = Pixels(seq, ui);
        ASSERT_TRUE(pixels) << seq;
        EXPECT_EQ(pixels->extent.width, seq == 1 ? 8u : static_cast<uint32_t>(15 + seq));
        Pixel(seq, ui, pixels->extent.width / 2, 4, {255, 0, 0});
    }
    EXPECT_FALSE(Find<RendererFailure>());
}

TEST_P(RendererRuntimeTest, B13TransparentFragmentsPreserveOrderAndClipAcrossQueuedFrames)
{
    const ViewportId ui{1, 1};
    auto             initial = Scene();
    AddMesh(*initial);
    ViewportDesc desc;
    desc.extent = {16, 8};
    initial->commands.push_back(CreateViewport{ui, desc});
    initial->featureCommands.push_back(RegisterRenderFeature{{0, 1}, Name("ui"), std::make_shared<int>(1), Ui()});
    Submit(std::move(initial));
    Complete(sequence);
    ASSERT_TRUE(Until([&] {
        return std::count_if(events.begin(), events.end(), [](const auto &event) {
                   const auto *state = std::get_if<ResourceStateChanged>(&event);
                   return state && state->state == RenderResourceState::RESIDENT;
               }) == 2;
    }));
    RenderUiSnapshot snapshot;
    snapshot.output = ui;
    for (uint32_t i = 0; i < 97; ++i) {
        const auto                 base = static_cast<uint32_t>(snapshot.vertices.size());
        const std::array<float, 4> tint{i % 2 ? 0.f : 1.f, 1, 1, .5f};
        snapshot.vertices.insert(snapshot.vertices.end(), {{0, 0, 0, 0, tint}, {1, 0, 1, 0, tint}, {0, 1, 0, 1, tint}, {1, 1, 1, 1, tint}});
        snapshot.indices.insert(snapshot.indices.end(), {base, base + 1, base + 2, base + 2, base + 1, base + 3});
        snapshot.draws.push_back({{viewport, 0}, i * 6, 6, {0, 0, .5f, 1}});
    }
    for (uint32_t i = 0; i < 3; ++i) {
        auto packet  = Packet();
        auto encoded = EncodeRenderUiSnapshot(Name("ui"), snapshot);
        ASSERT_TRUE(std::holds_alternative<FrameExtensionPayload>(encoded));
        packet->extensions.push_back(std::get<FrameExtensionPayload>(std::move(encoded)));
        packet->readbacks = {ui};
        Submit(std::move(packet));
    }
    Complete(sequence);
    for (uint64_t seq = 2; seq <= sequence; ++seq) {
        Pixel(seq, ui, 4, 4, {170, 0, 0});
        Pixel(seq, ui, 12, 4, {0, 0, 0});
    }
}

TEST_P(RendererRuntimeTest, B13StaleGenerationNeverSamplesAReplacementOutput)
{
    const ViewportId ui{1, 1};
    auto             initial = Scene();
    AddMesh(*initial);
    ViewportDesc desc;
    desc.extent = {8, 8};
    initial->commands.push_back(CreateViewport{ui, desc});
    initial->featureCommands.push_back(RegisterRenderFeature{{0, 1}, Name("ui"), std::make_shared<int>(1), Ui()});
    initial->extensions.push_back(UiSnapshot(ui, {viewport, 0}));
    initial->readbacks = {ui};
    Submit(std::move(initial));
    Complete(sequence);
    Submit(Packet({DestroyViewport{viewport}}));
    ASSERT_TRUE(Until([&] { return Find<ViewportDestroyed>(); }));
    const ViewportId replacement{0, 2};
    ViewDesc         camera;
    camera.scene  = scene;
    camera.output = replacement;
    auto next     = Packet({CreateViewport{replacement, desc}, UpdateView{view, camera, 1}});
    next->extensions.push_back(UiSnapshot(ui, {viewport, 0}));
    next->readbacks = {ui};
    Submit(std::move(next));
    Complete(sequence);
    Pixel(sequence, ui, 4, 4, {0, 0, 0});
    auto fresh = Packet();
    fresh->extensions.push_back(UiSnapshot(ui, {replacement, 0}));
    fresh->readbacks = {ui};
    Submit(std::move(fresh));
    Complete(sequence);
    Pixel(sequence, ui, 4, 4, {255, 0, 0});
}

TEST_P(RendererRuntimeTest, B13UnavailableNativeUiDoesNotRemoveTheSharedOffscreenProducer)
{
    WNDCLASSW cls{};
    cls.lpfnWndProc   = DefWindowProcW;
    cls.hInstance     = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"AuroraB13Ui";
    RegisterClassW(&cls);
    const auto window = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, cls.lpszClassName, L"Aurora B13", WS_POPUP, 0, 0, 64, 48, nullptr,
                                        nullptr, cls.hInstance, nullptr);
    ASSERT_TRUE(window);
    ShowWindow(window, SW_SHOWNOACTIVATE);
    const ViewportId native{1, 1}, ui{2, 1};
    auto             initial = Scene();
    AddMesh(*initial);
    ViewportDesc desc;
    desc.kind         = ViewportKind::NATIVE_WINDOW;
    desc.extent       = {64, 48};
    desc.format       = PixelFormat::BGRA8_UNORM;
    desc.nativeWindow = reinterpret_cast<uintptr_t>(window);
    initial->commands.push_back(CreateViewport{native, desc});
    desc        = {};
    desc.extent = {16, 8};
    initial->commands.push_back(CreateViewport{ui, desc});
    initial->featureCommands.push_back(RegisterRenderFeature{{0, 1}, Name("ui"), std::make_shared<int>(1), Ui()});
    initial->extensions = {UiSnapshot(native, {viewport, 0}), UiSnapshot(ui, {viewport, 0})};
    initial->readbacks  = {ui};
    Submit(std::move(initial));
    Complete(sequence);
    Pixel(sequence, ui, 8, 4, {255, 0, 0});
    ShowWindow(window, SW_MINIMIZE);
    auto next        = Packet();
    next->extensions = {UiSnapshot(native, {viewport, 0}), UiSnapshot(ui, {viewport, 0})};
    next->readbacks  = {ui};
    Submit(std::move(next));
    Complete(sequence);
    Pixel(sequence, ui, 8, 4, {255, 0, 0});
    auto close = Packet({DestroyViewport{native}});
    close->featureCommands.push_back(RetireRenderFeature{{0, 1}});
    Submit(std::move(close));
    const bool retired = Until([&] { return Find<ViewportDestroyed>() && Find<RenderFeatureRetired>(); });
    EXPECT_TRUE(retired);
    Stop();
    DestroyWindow(window);
}

TEST_P(RendererRuntimeTest, PublicCreateOwnsOneDeviceAndEpochChangesAfterStop)
{
    const auto epoch     = renderer->GetInfo().deviceEpoch;
    const auto duplicate = provider->Create(info);
    ASSERT_TRUE(std::holds_alternative<RendererError>(duplicate));
    EXPECT_EQ(std::get<RendererError>(duplicate).code, RendererErrorCode::NOT_READY);
    Stop();
    events.clear();
    Create();
    EXPECT_GT(renderer->GetInfo().deviceEpoch, epoch);
}

TEST_P(RendererRuntimeTest, EmptyFrameCompletesWithoutSubmissionAndQueueBackpressureRetainsOwnership)
{
    std::vector<uint64_t> accepted;
    for (uint32_t i = 0; i < info.maxPendingPackets + 1; ++i) {
        auto       packet   = Packet();
        const auto value    = packet->sequence;
        auto       result   = renderer->TrySubmit(packet);
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (result.status == PacketSubmitStatus::BUSY && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
            result = renderer->TrySubmit(packet);
        }
        ASSERT_EQ(result.status, PacketSubmitStatus::ACCEPTED);
        accepted.push_back(value);
    }
    auto blocked = Packet();
    EXPECT_EQ(renderer->PollEvents({}), 0u);
    EXPECT_EQ(renderer->TrySubmit(blocked).status, PacketSubmitStatus::BUSY);
    for (const auto value : accepted)
        Complete(value);
    Submit(std::move(blocked));
    Complete(sequence);
    for (const auto &event : events)
        if (const auto *complete = std::get_if<FrameCompleted>(&event))
            EXPECT_EQ(complete->submissionSerial, 0u);
}

TEST_P(RendererRuntimeTest, OffscreenSceneClearReadbackAndDestroyAcknowledgements)
{
    Submit(Scene());
    Complete(sequence);
    const auto *pixels = Find<ViewportReadback>();
    ASSERT_TRUE(pixels);
    ASSERT_EQ(pixels->bytes.size(), 8 * 8 * 4);
    for (size_t i = 0; i < pixels->bytes.size(); i += 4) {
        EXPECT_EQ(pixels->bytes[i], 0);
        EXPECT_EQ(pixels->bytes[i + 3], 255);
    }
    Submit(Packet({DestroyView{view}, DestroyViewport{viewport}, DestroyScene{scene}}));
    Complete(sequence);
    ASSERT_TRUE(Until([&] { return Find<ViewportDestroyed>() && Find<SceneDestroyed>() && Find<ViewDestroyed>(); }));
}

TEST_P(RendererRuntimeTest, FirstFrameUploadsExactCandidatesAndDrawsPixels)
{
    auto             packet = Scene();
    RenderEntityDesc entity;
    entity.mesh     = {Key(1201), 1};
    entity.material = {Key(1202), 1};
    packet->commands.emplace_back(CreateRenderEntity{{scene, {0, 1}}, entity});
    packet->commands.emplace_back(ResourceCpuReady{Mesh()});
    packet->commands.emplace_back(ResourceCpuReady{Material()});
    Submit(std::move(packet));
    Complete(sequence);
    const auto *pixels = Find<ViewportReadback>();
    ASSERT_TRUE(pixels);
    const auto center = (4 * 8 + 4) * 4;
    ASSERT_GT(pixels->bytes.size(), center + 3);
    EXPECT_EQ(pixels->bytes[center], 255);
    EXPECT_EQ(pixels->bytes[center + 1], 0);
    EXPECT_EQ(pixels->bytes[center + 3], 255);
    EXPECT_TRUE(Until([&] {
        return std::count_if(events.begin(), events.end(), [](const auto &event) {
                   const auto *resource = std::get_if<ResourceStateChanged>(&event);
                   return resource && resource->state == RenderResourceState::RESIDENT;
               }) == 2;
    }));
}

TEST_P(RendererRuntimeTest, LateResourcesBecomeResidentWithoutAnotherHostPacket)
{
    auto packet  = Packet();
    auto payload = Mesh();
    packet->commands.emplace_back(ResourceCpuReady{std::move(payload)});
    Submit(std::move(packet));
    Complete(sequence);
    EXPECT_TRUE(Until([&] {
        return std::any_of(events.begin(), events.end(), [](const auto &event) {
            const auto *resource = std::get_if<ResourceStateChanged>(&event);
            return resource && resource->state == RenderResourceState::RESIDENT;
        });
    }));
}

TEST_P(RendererRuntimeTest, ResizeAndHandleReuseRespectRetirement)
{
    Submit(Scene());
    Complete(sequence);
    events.clear();
    auto resize = Packet({ResizeViewport{viewport, {12, 10}, 1}});
    resize->readbacks.push_back(viewport);
    Submit(std::move(resize));
    Complete(sequence);
    const auto *pixels = Find<ViewportReadback>();
    ASSERT_TRUE(pixels);
    EXPECT_EQ(pixels->bytes.size(), 12 * 10 * 4);
    Submit(Packet({DestroyView{view}, DestroyViewport{viewport}}));
    ASSERT_TRUE(Until([&] { return Find<ViewportDestroyed>(); }));
    const ViewportId next{0, 2};
    const ViewId     nextView{0, 2};
    ViewDesc         camera;
    camera.scene  = scene;
    camera.output = next;
    ViewportDesc desc;
    desc.extent    = {4, 4};
    auto recreated = Packet({CreateViewport{next, desc}, CreateView{nextView, camera}, DestroyViewport{viewport}});
    recreated->readbacks.push_back(next);
    events.clear();
    Submit(std::move(recreated));
    Complete(sequence);
    ASSERT_TRUE(Find<ViewportReadback>());
    EXPECT_EQ(Find<ViewportReadback>()->viewport, next);
}

TEST_P(RendererRuntimeTest, StaleEpochRejectsWholePacketAndLaterPacketsContinue)
{
    auto bad     = Scene();
    auto payload = Mesh();
    ++payload.resource.deviceEpoch;
    bad->commands.push_back(ResourceCpuReady{std::move(payload)});
    Submit(std::move(bad));
    ASSERT_TRUE(Until([&] { return Find<RendererFailure>(); }));
    EXPECT_FALSE(Find<PacketApplied>());
    EXPECT_EQ(renderer->GetState(), RendererState::RUNNING);
    events.clear();
    Submit(Scene());
    Complete(sequence);
    EXPECT_TRUE(Find<ViewportReadback>());
}

TEST_P(RendererRuntimeTest, SceneProjectionFailureIsIsolatedAndViewUpdateResumesIt)
{
    auto packet = Scene();
    packet->readbacks.clear();
    auto &camera           = std::get<CreateView>(packet->commands.back()).desc;
    camera.view.v[0]       = std::numeric_limits<float>::max();
    camera.projection.v[0] = std::numeric_limits<float>::max();
    Submit(std::move(packet));
    Complete(sequence);
    EXPECT_TRUE(Find<ScenePrepareFailed>());
    EXPECT_EQ(renderer->GetState(), RendererState::RUNNING);
    ViewDesc fixed;
    fixed.scene  = scene;
    fixed.output = viewport;
    auto resume  = Packet({UpdateView{view, fixed, 1}});
    resume->readbacks.push_back(viewport);
    events.clear();
    Submit(std::move(resume));
    Complete(sequence);
    EXPECT_TRUE(Find<ViewportReadback>());
    EXPECT_FALSE(Find<ScenePrepareFailed>());
}

TEST_P(RendererRuntimeTest, NativeWindowAndOffscreenSubmitTogetherAndCloseSeparately)
{
    WNDCLASSW cls{};
    cls.lpfnWndProc   = DefWindowProcW;
    cls.hInstance     = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"AuroraB12Runtime";
    RegisterClassW(&cls);
    const auto window = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, cls.lpszClassName, L"Aurora B12", WS_POPUP, 0, 0, 64, 64, nullptr,
                                        nullptr, cls.hInstance, nullptr);
    ASSERT_TRUE(window);
    ShowWindow(window, SW_SHOWNOACTIVATE);
    const ViewportId native{1, 1};
    auto             packet = Scene();
    ViewportDesc     desc;
    desc.kind         = ViewportKind::NATIVE_WINDOW;
    desc.extent       = {64, 64};
    desc.format       = PixelFormat::BGRA8_UNORM;
    desc.nativeWindow = reinterpret_cast<uintptr_t>(window);
    ViewDesc camera;
    camera.scene  = scene;
    camera.output = native;
    packet->commands.push_back(CreateViewport{native, desc});
    packet->commands.push_back(CreateView{{1, 1}, camera});
    packet->commands.push_back(ResourceCpuReady{Mesh()});
    Submit(std::move(packet));
    Complete(sequence);
    EXPECT_TRUE(Find<ViewportReadback>());
    ShowWindow(window, SW_MINIMIZE);
    auto minimized = Packet();
    auto late      = Mesh();
    late.resource  = Key(1203);
    minimized->commands.push_back(ResourceCpuReady{std::move(late)});
    minimized->readbacks.push_back(viewport);
    events.clear();
    Submit(std::move(minimized));
    Complete(sequence);
    EXPECT_TRUE(Find<ViewportReadback>());
    ShowWindow(window, SW_SHOWNOACTIVATE);
    auto close = Packet({DestroyViewport{native}});
    Submit(std::move(close));
    const bool closed = Until([&] {
        return std::any_of(events.begin(), events.end(), [&](const auto &event) {
            const auto *destroyed = std::get_if<ViewportDestroyed>(&event);
            return destroyed && destroyed->viewport == native;
        });
    });
    EXPECT_TRUE(closed);
    if (!closed)
        Stop();
    DestroyWindow(window);
    if (!closed)
        return;
    events.clear();
    auto next = Packet();
    next->readbacks.push_back(viewport);
    Submit(std::move(next));
    Complete(sequence);
    EXPECT_TRUE(Find<ViewportReadback>());
}

TEST_P(RendererRuntimeTest, EncodingFailureDiscardsOnlyTheFrameAndFollowingFrameCanRender)
{
    {
        EncodeFault fault(*Instance::Get()->GetDevice());
        Submit(Scene());
        ASSERT_TRUE(Until([&] { return Find<RendererFailure>(); }));
        EXPECT_TRUE(fault.triggered);
        EXPECT_FALSE(Find<ViewportReadback>());
        EXPECT_EQ(renderer->GetState(), RendererState::RUNNING);
    }
    events.clear();
    auto next = Packet();
    next->readbacks.push_back(viewport);
    Submit(std::move(next));
    Complete(sequence);
    EXPECT_TRUE(Find<ViewportReadback>());
}

TEST_P(RendererRuntimeTest, ChunkBudgetDefersFrozenDrawUntilItsExactResourcesAreResident)
{
    Stop();
    events.clear();
    info.maxUploadBytesPerFrame = 16;
    Create();
    auto             packet = Scene();
    RenderEntityDesc entity;
    entity.mesh     = {Key(1201), 1};
    entity.material = {Key(1202), 1};
    packet->commands.emplace_back(CreateRenderEntity{{scene, {0, 1}}, entity});
    packet->commands.emplace_back(ResourceCpuReady{Mesh()});
    packet->commands.emplace_back(ResourceCpuReady{Material()});
    Submit(std::move(packet));
    Complete(sequence);
    ASSERT_TRUE(Find<ViewportReadback>());
    EXPECT_EQ(Find<ViewportReadback>()->bytes[(4 * 8 + 4) * 4], 0);
    ASSERT_TRUE(Until([&] {
        return std::count_if(events.begin(), events.end(), [](const auto &event) {
                   const auto *resource = std::get_if<ResourceStateChanged>(&event);
                   return resource && resource->state == RenderResourceState::RESIDENT;
               }) == 2;
    }));
    events.clear();
    auto next = Packet();
    next->readbacks.push_back(viewport);
    Submit(std::move(next));
    Complete(sequence);
    ASSERT_TRUE(Find<ViewportReadback>());
    EXPECT_EQ(Find<ViewportReadback>()->bytes[(4 * 8 + 4) * 4], 255);
}

TEST_P(RendererRuntimeTest, SharedUploadSurvivesOneSubscriberAndCancelsAfterTheLast)
{
    Stop();
    events.clear();
    info.maxUploadBytesPerFrame = 16;
    Create();
    auto             packet = Scene();
    RenderEntityDesc entity;
    entity.mesh = {Key(1201), 1};
    packet->commands.push_back(CreateRenderEntity{{scene, {0, 1}}, entity});
    packet->commands.push_back(CreateRenderEntity{{scene, {1, 1}}, entity});
    auto payload = Mesh();
    payload.bytes.resize(65536);
    packet->commands.push_back(ResourceCpuReady{std::move(payload)});
    Submit(std::move(packet));
    Complete(sequence);
    events.clear();
    Submit(Packet({DestroyRenderEntity{{scene, {0, 1}}}}));
    Complete(sequence);
    for (const auto &event : events)
        if (const auto *resource = std::get_if<ResourceStateChanged>(&event))
            EXPECT_NE(resource->state, RenderResourceState::CANCELLED);
    events.clear();
    Submit(Packet({DestroyRenderEntity{{scene, {1, 1}}}}));
    Complete(sequence);
    EXPECT_TRUE(Until([&] {
        return std::any_of(events.begin(), events.end(), [](const auto &event) {
            const auto *resource = std::get_if<ResourceStateChanged>(&event);
            return resource && resource->state == RenderResourceState::CANCELLED;
        });
    }));
}

TEST_P(RendererRuntimeTest, HotUpdateUsesTheFrozenOldMaterialUntilTheNewVersionIsResident)
{
    auto             packet = Scene();
    RenderEntityDesc entity;
    entity.mesh     = {Key(1201), 1};
    entity.material = {Key(1202), 1};
    packet->commands.push_back(CreateRenderEntity{{scene, {0, 1}}, entity});
    packet->commands.push_back(ResourceCpuReady{Mesh()});
    packet->commands.push_back(ResourceCpuReady{Material()});
    Submit(std::move(packet));
    Complete(sequence);
    events.clear();
    auto updated              = Material();
    updated.resource.revision = 2;
    const float green[]       = {0, 1, 0, 1};
    std::memcpy(updated.bytes.data(), green, sizeof(green));
    entity.material = {updated.resource, 2};
    auto update     = Packet({UpdateRenderEntity{{scene, {0, 1}}, entity, 1}, ResourceCpuReady{std::move(updated)}});
    update->readbacks.push_back(viewport);
    Submit(std::move(update));
    Complete(sequence);
    ASSERT_TRUE(Find<ViewportReadback>());
    EXPECT_EQ(Find<ViewportReadback>()->bytes[(4 * 8 + 4) * 4], 255);
    ASSERT_TRUE(Until([&] {
        return std::any_of(events.begin(), events.end(), [](const auto &event) {
            const auto *resource = std::get_if<ResourceStateChanged>(&event);
            return resource && resource->resource.revision == 2 && resource->state == RenderResourceState::RESIDENT;
        });
    }));
    events.clear();
    auto next = Packet();
    next->readbacks.push_back(viewport);
    Submit(std::move(next));
    Complete(sequence);
    ASSERT_TRUE(Find<ViewportReadback>());
    EXPECT_EQ(Find<ViewportReadback>()->bytes[(4 * 8 + 4) * 4], 0);
    EXPECT_EQ(Find<ViewportReadback>()->bytes[(4 * 8 + 4) * 4 + 1], 255);
}

TEST(ViewportMappingTest, AllTransformsUseTheSameCornersForClipAndUiCoordinates)
{
    const std::array<Vector2, 8> expected{
        {{0.2f, 0.3f}, {0.7f, 0.2f}, {0.8f, 0.7f}, {0.3f, 0.8f}, {0.8f, 0.3f}, {0.7f, 0.8f}, {0.2f, 0.7f}, {0.3f, 0.2f}}};
    for (uint32_t i = 0; i < expected.size(); ++i) {
        const ViewportMapping mapping{{80, 40}, static_cast<SurfaceTransform>(i)};
        const auto            point = mapping.ToImage({0.2f, 0.3f});
        const auto            clip  = mapping.ClipTransform() * Vector4{-0.6f, -0.4f, 0.2f, 1};
        EXPECT_NEAR(point.x, expected[i].x, 1e-6f);
        EXPECT_NEAR(point.y, expected[i].y, 1e-6f);
        EXPECT_NEAR(clip.x, point.x * 2 - 1, 1e-6f);
        EXPECT_NEAR(clip.y, point.y * 2 - 1, 1e-6f);
        EXPECT_FLOAT_EQ(clip.z, 0.2f);
        EXPECT_EQ(mapping.LogicalExtent().width, i % 2 ? 40u : 80u);
        EXPECT_EQ(mapping.LogicalExtent().height, i % 2 ? 80u : 40u);
    }
}

TEST_P(RendererRuntimeTest, InvalidWorkerConfigurationAndWrongThreadSubmissionAreRejected)
{
    auto invalid          = info;
    invalid.encodeWorkers = UINT32_MAX;
    auto creation         = provider->Create(invalid);
    ASSERT_TRUE(std::holds_alternative<RendererError>(creation));
    EXPECT_EQ(std::get<RendererError>(creation).code, RendererErrorCode::INVALID_ARGUMENT);
    auto               packet = Packet();
    PacketSubmitResult result;
    std::thread        other([&] { result = renderer->TrySubmit(packet); });
    other.join();
    EXPECT_EQ(result.status, PacketSubmitStatus::REJECTED);
    EXPECT_TRUE(packet);
    EXPECT_EQ(renderer->PollEvents({}), 0u);
    Submit(std::move(packet));
    Complete(sequence);
}

TEST_P(RendererRuntimeTest, QueuedResizeFramesRetainTheirOwnOutputVersions)
{
    Stop();
    events.clear();
    info.inflightFrames    = 1;
    info.maxPendingPackets = 16;
    Create();
    Submit(Scene());
    for (uint32_t size = 9; size <= 16; ++size) {
        auto packet = Packet({ResizeViewport{viewport, {size, size + 1}, size - 8}});
        packet->readbacks.push_back(viewport);
        Submit(std::move(packet));
    }
    renderer->RequestShutdown();
    Complete(sequence);
    ASSERT_TRUE(Until([&] {
        return std::count_if(events.begin(), events.end(), [](const auto &event) { return std::holds_alternative<ViewportReadback>(event); }) == 9;
    }));
    for (const auto &event : events) {
        if (const auto *readback = std::get_if<ViewportReadback>(&event)) {
            const auto width = static_cast<uint32_t>(readback->sequence + 7);
            EXPECT_EQ(readback->extent.width, width);
            EXPECT_EQ(readback->extent.height, width == 8 ? 8u : width + 1);
            EXPECT_EQ(readback->bytes.size(), uint64_t(readback->extent.width) * readback->extent.height * 4);
        }
    }
    EXPECT_TRUE(Until([&] { return Find<RendererStopped>(); }));
}

TEST_P(RendererRuntimeTest, PrepareKernelFailurePausesOnlyItsSceneAndCanResume)
{
    auto packet = Scene();
    packet->readbacks.clear();
    RenderEntityDesc invalid;
    invalid.world.v[12]      = std::numeric_limits<float>::max();
    invalid.properties.light = {{1, 1, 1}, 1, std::numeric_limits<float>::max()};
    packet->commands.push_back(CreateRenderEntity{{scene, {0, 1}}, invalid});
    Submit(std::move(packet));
    ASSERT_TRUE(Until([&] { return Find<ScenePrepareFailed>() && Find<RendererFailure>(); }));
    EXPECT_EQ(renderer->GetState(), RendererState::RUNNING);
    events.clear();
    const SceneId other{1, 1};
    ViewDesc      camera;
    camera.scene  = other;
    camera.output = viewport;
    auto next     = Packet({CreateScene{other, {}}, CreateView{{1, 1}, camera}});
    next->readbacks.push_back(viewport);
    Submit(std::move(next));
    Complete(sequence);
    EXPECT_TRUE(Find<ViewportReadback>());
    EXPECT_FALSE(Find<ScenePrepareFailed>());
    events.clear();
    auto resume = Packet({UpdateRenderEntity{{scene, {0, 1}}, {}, 1}, DestroyView{{1, 1}}});
    resume->readbacks.push_back(viewport);
    Submit(std::move(resume));
    Complete(sequence);
    EXPECT_TRUE(Find<ViewportReadback>());
}

TEST_P(RendererRuntimeTest, DeviceLossDrainsAcceptedPacketsAndAllowsAReplacementRenderer)
{
    const auto epoch = renderer->GetInfo().deviceEpoch;
    {
        EncodeFault fault(*Instance::Get()->GetDevice(), true);
        Submit(Scene());
        ASSERT_TRUE(Until([&] { return Find<RendererStopped>(); }));
        ASSERT_TRUE(Find<RendererFailure>());
        EXPECT_EQ(Find<RendererFailure>()->error.code, RendererErrorCode::DEVICE_LOST);
        EXPECT_EQ(std::count_if(events.begin(), events.end(),
                                [&](const auto &event) {
                                    const auto *failure = std::get_if<RendererFailure>(&event);
                                    return failure && failure->packetSequence == sequence;
                                }),
                  1);
        EXPECT_FALSE(Find<ViewportReadback>());
        Stop();
    }
    events.clear();
    Create();
    EXPECT_GT(renderer->GetInfo().deviceEpoch, epoch);
    Submit(Scene());
    Complete(sequence);
}

INSTANTIATE_TEST_SUITE_P(Windows, RendererRuntimeTest, testing::Values(0u, 1u, 2u));

TEST_P(RendererRuntimeTest, B14OwnedAtlasRevisionsRoundedClippingAndRelease)
{
    editor::UiSnapshotProducer producer(renderer->GetInfo().deviceEpoch);
    ui::UIImageData            red{2, 2, std::vector<uint8_t>(16)};
    for (size_t i = 0; i < 16; i += 4) {
        red.pixels[i]     = 255;
        red.pixels[i + 3] = 255;
    }
    const auto texture = producer.RegisterTexture(red);
    ASSERT_GE(texture, 2u);
    auto packet = Packet({CreateViewport{viewport, {ViewportKind::OFFSCREEN, {32, 32}}}});
    packet->featureCommands.push_back(RegisterRenderFeature{{0, 1}, Name("ui"), std::make_shared<int>(1), Ui()});
    ui::UIPaintContext paint;
    const auto         snapshot = [&](FramePacket &target) {
        paint.Begin({0, 0, 32, 32});
        paint.AddTexturedQuad({0, 0, 32, 32}, {0, 0, 1, 1}, texture, 0xFFFFFFFF);
        paint.PushClip({0, 0, 16, 32});
        paint.AddRoundedRect({4, 4, 28, 28}, 0xFF00FF00, 8);
        paint.PopClip();
        auto encoded = producer.Snapshot(Name("ui"), viewport, {32, 32}, paint.GetDrawData(), {});
        EXPECT_TRUE(std::holds_alternative<FrameExtensionPayload>(encoded));
        target.extensions.push_back(std::get<FrameExtensionPayload>(std::move(encoded)));
        target.readbacks.push_back(viewport);
        producer.Flush(target);
    };
    snapshot(*packet);
    Submit(std::move(packet));
    Complete(sequence);
    const auto *first = Pixels(sequence, viewport);
    ASSERT_TRUE(first);
    EXPECT_GT(first->bytes[(16 * 32 + 8) * 4 + 1], 240); // Inside the rounded shape.
    EXPECT_GT(first->bytes[(16 * 32 + 24) * 4], 240);    // Outside the clip.
    EXPECT_GT(first->bytes[(4 * 32 + 4) * 4], 240);      // Rounded corner preserves the image.
    auto blue = red;
    for (size_t i = 0; i < 16; i += 4) {
        blue.pixels[i]     = 0;
        blue.pixels[i + 2] = 255;
    }
    producer.UpdateTexture(texture, blue);
    packet = Packet();
    snapshot(*packet);
    ASSERT_EQ(packet->releasedResources.size(), 1u);
    Submit(std::move(packet));
    Complete(sequence);
    const auto *second = Pixels(sequence, viewport);
    ASSERT_TRUE(second);
    EXPECT_GT(second->bytes[(16 * 32 + 24) * 4 + 2], 240);
    EXPECT_LT(second->bytes[(16 * 32 + 24) * 4], 10);
    producer.ReleaseTexture(texture);
    packet = Packet();
    producer.Flush(*packet);
    ASSERT_EQ(packet->releasedResources.size(), 1u);
    Submit(std::move(packet));
    Complete(sequence);
    EXPECT_EQ(producer.CpuBytes(), 4u);
}

TEST(UiSnapshotProducerTest, B14InvalidAtlasUpdateReportsErrorWithoutReplacingOwnedPixels)
{
    editor::UiSnapshotProducer producer(1);
    const auto                 texture = producer.RegisterTexture({1, 1, {255, 0, 0, 255}});
    ASSERT_GE(texture, 2u);
    producer.UpdateTexture(texture, {2, 2, {0, 0, 255, 255}});
    EXPECT_EQ(producer.CpuBytes(), 8u);
    const auto result = producer.Snapshot(Name("ui"), {0, 1}, {32, 32}, {}, {});
    ASSERT_TRUE(std::holds_alternative<RendererError>(result));
    EXPECT_EQ(std::get<RendererError>(result).code, RendererErrorCode::INVALID_ARGUMENT);
}

TEST_P(RendererRuntimeTest, DepthOnlySelectedSubresourceIsSampledAfterParallelDrawsAndLoad)
{
    auto packet = Packet({CreateViewport{viewport, {ViewportKind::OFFSCREEN, {8, 8}}}});
    packet->featureCommands.push_back(
        RegisterRenderFeature{{0, 1}, Name("depth.sample"), std::make_shared<int>(1), std::make_shared<DepthSamplingFeature>(renderer->GetInfo())});
    FrameExtensionPayload payload;
    payload.feature = Name("depth.sample");
    payload.outputs = {viewport};
    packet->extensions.push_back(std::move(payload));
    packet->readbacks.push_back(viewport);
    Submit(std::move(packet));
    Complete(sequence);
    const auto *pixels = Pixels(sequence, viewport);
    ASSERT_TRUE(pixels);
    EXPECT_NEAR(pixels->bytes[(4 * 8 + 2) * 4], 64, 1);
    EXPECT_EQ(pixels->bytes[(4 * 8 + 6) * 4], 255);
}

TEST_P(RendererRuntimeTest, OffCameraCasterAndLightShadowFlagsChangeReceiverPixels)
{
    struct CompilerScope {
        ShaderCompilerSlang compiler;
        IShaderCompiler    *previous = ShaderCompilerFactory::Get().GetCompiler();
        CompilerScope()
        {
            ShaderCompilerFactory::Get().Register(&compiler);
        }
        ~CompilerScope()
        {
            ShaderCompilerFactory::Get().Register(previous);
        }
    } compiler;
    ShaderFileSystem files;
    RegisterSceneShaderSources(files);
    ShaderRef reference;
    reference.relativePath = "SceneLit.slang";
    const auto compiled    = CompileSceneMaterial(files, info.backend, reference);
    ASSERT_TRUE(std::holds_alternative<RenderMaterialData>(compiled))
        << (std::get_if<RendererError>(&compiled) ? std::get<RendererError>(compiled).message : "");
    auto mesh = Mesh(), material = Material();
    material.bytes           = SceneBatchBytes({1, 1, 1, 1});
    material.material        = std::get<RenderMaterialData>(compiled);
    material.material->depth = {true, true, CompareOp::LESS_OR_EQUAL};
    RenderEntityDesc receiver;
    receiver.mesh                     = {mesh.resource, 1};
    receiver.material                 = {material.resource, 1};
    receiver.world.v[14]              = .8f;
    receiver.properties.receiveShadow = true;
    auto caster                       = receiver;
    caster.world.v[0] = caster.world.v[5] = .35f;
    caster.world.v[12]                    = 1.6f;
    caster.world.v[14]                    = .2f;
    caster.properties.castShadow          = true;
    RenderEntityDesc light;
    light.properties.light.kind       = RenderLightKind::DIRECTIONAL;
    light.properties.light.direction  = {-0.936329f, 0, 0.351123f};
    light.properties.light.intensity  = 2;
    light.properties.light.castShadow = true;
    auto  initial                     = Scene();
    auto &camera                      = std::get<CreateView>(initial->commands[2]).desc;
    camera.cameraPosition             = {0, 0, -1};
    initial->commands.emplace_back(CreateRenderEntity{{scene, {0, 1}}, receiver});
    initial->commands.emplace_back(CreateRenderEntity{{scene, {1, 1}}, caster});
    initial->commands.emplace_back(CreateRenderEntity{{scene, {2, 1}}, light});
    initial->commands.emplace_back(ResourceCpuReady{std::move(mesh)});
    initial->commands.emplace_back(ResourceCpuReady{std::move(material)});
    Submit(std::move(initial));
    Complete(sequence);
    const auto sample = [&](std::vector<RenderCommand> commands) {
        auto packet = Packet(std::move(commands));
        packet->readbacks.push_back(viewport);
        Submit(std::move(packet));
        Complete(sequence);
        const auto *pixels = Pixels(sequence, viewport);
        EXPECT_NE(pixels, nullptr);
        return pixels ? pixels->bytes[(4 * 8 + 4) * 4] : uint8_t{0};
    };
    const auto shadowed = sample({});
    EXPECT_NEAR(shadowed, 8, 2);
    light.properties.light.castShadow = false;
    const auto lit                    = sample({UpdateRenderEntity{{scene, {2, 1}}, light, 1}});
    EXPECT_GT(lit, 150);
    light.properties.light.intensity = 0;
    const auto dark                  = sample({UpdateRenderEntity{{scene, {2, 1}}, light, 2}});
    EXPECT_NEAR(dark, 8, 2);
    light.properties.light.intensity  = 2;
    light.properties.light.castShadow = true;
    receiver.properties.receiveShadow = false;
    EXPECT_NEAR(sample({UpdateRenderEntity{{scene, {2, 1}}, light, 3}, UpdateRenderEntity{{scene, {0, 1}}, receiver, 1}}), lit, 2);
    receiver.properties.receiveShadow = true;
    EXPECT_NEAR(sample({UpdateRenderEntity{{scene, {0, 1}}, receiver, 2}, DestroyRenderEntity{{scene, {1, 1}}}}), lit, 2);
    EXPECT_TRUE(Until([&] { return Find<EntityDestroyed>() != nullptr; }));
    Stop();
}

TEST_P(RendererRuntimeTest, ScenePositiveYStaysAboveCenterWithAndWithoutHdr)
{
    struct CompilerScope {
        ShaderCompilerSlang compiler;
        IShaderCompiler    *previous = ShaderCompilerFactory::Get().GetCompiler();
        CompilerScope()
        {
            ShaderCompilerFactory::Get().Register(&compiler);
        }
        ~CompilerScope()
        {
            ShaderCompilerFactory::Get().Register(previous);
        }
    } compiler;
    ShaderFileSystem files;
    RegisterSceneShaderSources(files);
    const auto compiled = CompileSceneMaterial(files, info.backend, {});
    ASSERT_TRUE(std::holds_alternative<RenderMaterialData>(compiled));
    auto material            = Material();
    material.material        = std::get<RenderMaterialData>(compiled);
    material.bytes           = SceneBatchBytes({1, 0, 0, 1});
    auto             initial = Scene();
    RenderEntityDesc entity;
    entity.mesh       = {Key(1201), 1};
    entity.material   = {material.resource, 1};
    entity.world.v[0] = entity.world.v[5] = .25f;
    entity.world.v[13]                    = .5f;
    initial->commands.emplace_back(CreateRenderEntity{{scene, {0, 1}}, entity});
    initial->commands.emplace_back(ResourceCpuReady{Mesh()});
    initial->commands.emplace_back(ResourceCpuReady{std::move(material)});
    Submit(std::move(initial));
    Complete(sequence);
    for (uint32_t hdr : {0u, 1u}) {
        auto packet = Packet();
        packet->extensions.push_back(SceneViewInput(Name("aurora.scene"), viewport, {hdr, 0}));
        packet->readbacks.push_back(viewport);
        Submit(std::move(packet));
        Complete(sequence);
        ASSERT_TRUE(Until([&] { return Pixels(sequence, viewport) != nullptr; }));
        Pixel(sequence, viewport, 4, 1, {hdr ? 188 : 255, 0, 0});
        Pixel(sequence, viewport, 4, 6, {0, 0, 0});
    }
    Stop();
}

TEST_P(RendererRuntimeTest, HdrExposureAndResizeKeepQueuedSceneValuesIndependent)
{
    struct CompilerScope {
        ShaderCompilerSlang compiler;
        IShaderCompiler    *previous = ShaderCompilerFactory::Get().GetCompiler();
        CompilerScope()
        {
            ShaderCompilerFactory::Get().Register(&compiler);
        }
        ~CompilerScope()
        {
            ShaderCompilerFactory::Get().Register(previous);
        }
    } compiler;
    ShaderFileSystem files;
    RegisterSceneShaderSources(files);
    const auto compiled = CompileSceneMaterial(files, info.backend, {});
    ASSERT_TRUE(std::holds_alternative<RenderMaterialData>(compiled));
    auto material            = Material();
    material.material        = std::get<RenderMaterialData>(compiled);
    material.bytes           = SceneBatchBytes({4, 1, .25f, 1});
    auto             initial = Scene();
    RenderEntityDesc entity;
    entity.mesh     = {Key(1201), 1};
    entity.material = {material.resource, 1};
    initial->commands.emplace_back(CreateRenderEntity{{scene, {0, 1}}, entity});
    initial->commands.emplace_back(ResourceCpuReady{Mesh()});
    initial->commands.emplace_back(ResourceCpuReady{std::move(material)});
    Submit(std::move(initial));
    Complete(sequence);
    auto first = Packet();
    first->extensions.push_back(SceneViewInput(Name("aurora.scene"), viewport, {1, 0}));
    first->readbacks.push_back(viewport);
    const auto baseline = first->sequence;
    Submit(std::move(first));
    auto next = Packet({ResizeViewport{viewport, {12, 6}, 1}});
    next->extensions.push_back(SceneViewInput(Name("aurora.scene"), viewport, {1, -2}));
    next->readbacks.push_back(viewport);
    Submit(std::move(next));
    Complete(sequence);
    ASSERT_TRUE(Until([&] { return Pixels(baseline, viewport) && Pixels(sequence, viewport); }));
    const auto *before = Pixels(baseline, viewport);
    const auto *after  = Pixels(sequence, viewport);
    ASSERT_EQ(before->bytes.size(), 8u * 8u * 4u);
    ASSERT_EQ(after->bytes.size(), 12u * 6u * 4u);
    const auto *a = before->bytes.data() + (4 * 8 + 4) * 4;
    const auto *b = after->bytes.data() + (3 * 12 + 6) * 4;
    EXPECT_NEAR(a[0], 231, 2);
    EXPECT_NEAR(a[1], 188, 2);
    EXPECT_NEAR(a[2], 124, 2);
    EXPECT_NEAR(b[0], 188, 2);
    EXPECT_NEAR(b[1], 124, 2);
    EXPECT_NEAR(b[2], 69, 2);
    Stop();
}

TEST_P(RendererRuntimeTest, EnvironmentInputsUseNeutralDefaultsExactVersionsAndPrefilteredMip)
{
    struct CompilerScope {
        ShaderCompilerSlang compiler;
        IShaderCompiler    *previous = ShaderCompilerFactory::Get().GetCompiler();
        CompilerScope()
        {
            ShaderCompilerFactory::Get().Register(&compiler);
        }
        ~CompilerScope()
        {
            ShaderCompilerFactory::Get().Register(previous);
        }
    } compiler;
    ShaderFileSystem files;
    RegisterSceneShaderSources(files);
    ShaderRef reference;
    reference.relativePath = "SceneLit.slang";
    const auto compiled    = CompileSceneMaterial(files, info.backend, reference);
    ASSERT_TRUE(std::holds_alternative<RenderMaterialData>(compiled));
    auto material                                                  = Material();
    material.material                                              = std::get<RenderMaterialData>(compiled);
    material.bytes                                                 = SceneBatchBytes({1, 1, 1, 1});
    auto initial                                                   = Scene();
    std::get<CreateView>(initial->commands[2]).desc.cameraPosition = {0, 0, -1};
    RenderEntityDesc entity;
    entity.world.v[14] = .5f;
    entity.mesh        = {Key(1201), 1};
    entity.material    = {material.resource, 1};
    initial->commands.emplace_back(CreateRenderEntity{{scene, {0, 1}}, entity});
    initial->commands.emplace_back(ResourceCpuReady{Mesh()});
    initial->commands.emplace_back(ResourceCpuReady{std::move(material)});
    Submit(std::move(initial));
    Complete(sequence);
    const auto image = [&](RenderResourceKey key, RenderImageLayout layout, std::array<uint16_t, 4> color) {
        RenderResourcePayload payload;
        payload.resource       = key;
        payload.format         = Name("linear-environment");
        payload.metadata.image = layout;
        payload.bytes.resize(*RenderImageByteSize(layout));
        for (size_t offset = 0; offset < payload.bytes.size(); offset += sizeof(color))
            std::memcpy(payload.bytes.data() + offset, color.data(), sizeof(color));
        return payload;
    };
    const auto queue = [&](SceneEnvironmentInputs inputs, std::vector<RenderCommand> commands = {}, float intensity = 1.f) {
        auto packet = Packet(std::move(commands));
        packet->extensions.push_back(SceneViewInput(Name("aurora.scene"), viewport, {1, 0, intensity}, inputs));
        packet->readbacks.push_back(viewport);
        const auto submitted = packet->sequence;
        Submit(std::move(packet));
        return submitted;
    };
    const auto expect = [&](uint64_t submitted, std::array<uint8_t, 3> expected) {
        ASSERT_TRUE(Until([&] { return Pixels(submitted, viewport) != nullptr; }));
        const auto *pixel = Pixels(submitted, viewport)->bytes.data() + (4 * 8 + 4) * 4;
        for (size_t i = 0; i < expected.size(); ++i)
            EXPECT_NEAR(pixel[i], expected[i], 2);
    };
    SceneEnvironmentInputs inputs{Key(1301), {}, {}};
    const auto             pending = queue(inputs);
    Complete(pending);
    expect(pending, {48, 48, 48});
    const RenderImageLayout imageLayout{2, 1, 2, 1, RenderImageFormat::RGBA16_FLOAT};
    // Half-float constants: 4, 1 and 0.5 preserve radiance above the UNORM range.
    const auto red   = queue(inputs, {ResourceCpuReady{image(Key(1301), imageLayout, {0x4400, 0, 0, 0x3c00})}});
    inputs[0]        = Key(1301, 2);
    const auto green = queue(inputs, {ResourceCpuReady{image(Key(1301, 2), imageLayout, {0, 0x3c00, 0, 0x3c00})}});
    Complete(green);
    expect(red, {231, 0, 0});
    expect(green, {0, 188, 0});
    auto                          specular = image(Key(1303), {4, 2, 3, 1, RenderImageFormat::RGBA16_FLOAT}, {0, 0, 0, 0x3c00});
    const std::array<uint16_t, 4> blue{0, 0, 0x4e40, 0x3c00}; // 25 in mip 1; base and final mip remain black.
    for (size_t texel = 8; texel < 10; ++texel)
        std::memcpy(specular.bytes.data() + texel * 8, blue.data(), 8);
    inputs          = {Key(1302), Key(1303), Key(1304)};
    const auto spec = queue(inputs, {ResourceCpuReady{image(Key(1302), imageLayout, {0, 0, 0, 0x3c00})}, ResourceCpuReady{std::move(specular)},
                                     ResourceCpuReady{image(Key(1304), imageLayout, {0x3800, 0, 0, 0x3c00})}});
    Complete(spec);
    expect(spec, {0, 0, 156});
    const auto disabled = queue(inputs, {}, 0);
    Complete(disabled);
    expect(disabled, {0, 0, 0});
    Stop();
}

TEST_P(RendererRuntimeTest, UiRepeatedBindingsShareDescriptorsAcrossDifferentClips)
{
    editor::UiSnapshotProducer producer(renderer->GetInfo().deviceEpoch);
    auto                       packet = Packet({CreateViewport{viewport, {ViewportKind::OFFSCREEN, {32, 32}}}});
    packet->featureCommands.push_back(RegisterRenderFeature{{0, 1}, Name("ui"), std::make_shared<int>(1), Ui()});
    ui::UIPaintContext paint;
    paint.Begin({0, 0, 32, 32});
    for (uint32_t i = 0; i < 300; ++i) {
        paint.PushClip({0, 0, i % 2 ? 31.f : 32.f, 32});
        paint.AddRoundedRect({0, 0, 32, 32}, 0xFF00FF00, 2);
        paint.PopClip();
    }
    ASSERT_GT(paint.GetDrawData().commands.size(), 128u);
    auto encoded = producer.Snapshot(Name("ui"), viewport, {32, 32}, paint.GetDrawData(), {});
    ASSERT_TRUE(std::holds_alternative<FrameExtensionPayload>(encoded));
    packet->extensions.push_back(std::get<FrameExtensionPayload>(std::move(encoded)));
    packet->readbacks.push_back(viewport);
    producer.Flush(*packet);
    Submit(std::move(packet));
    Complete(sequence);
    const auto *pixels = Pixels(sequence, viewport);
    ASSERT_TRUE(pixels);
    EXPECT_GT(pixels->bytes[(16 * 32 + 16) * 4 + 1], 240);
}

TEST_P(RendererRuntimeTest, TransformedRoundedUiUsesLayoutCoordinatesAndSurfaceClip)
{
    editor::UiSnapshotProducer producer(renderer->GetInfo().deviceEpoch);
    auto                       packet = Packet({CreateViewport{viewport, {ViewportKind::OFFSCREEN, {64, 64}}}});
    packet->featureCommands.push_back(RegisterRenderFeature{{0, 1}, Name("ui"), std::make_shared<int>(1), Ui()});
    ui::UIPaintContext paint;
    paint.Begin({0, 0, 64, 64});
    paint.AddRect({0, 0, 64, 64}, 0xFF0000FF);
    paint.PushTransform({2, 0, 8, 0, 1.5f, 10});
    paint.PushClip({4, 4, 12, 20});
    paint.AddRoundedRect({4, 4, 20, 20}, 0xFF00FF00, 4);
    paint.PopClip();
    paint.PopTransform();
    auto encoded = producer.Snapshot(Name("ui"), viewport, {64, 64}, paint.GetDrawData(), {});
    ASSERT_TRUE(std::holds_alternative<FrameExtensionPayload>(encoded));
    packet->extensions.push_back(std::get<FrameExtensionPayload>(std::move(encoded)));
    packet->readbacks.push_back(viewport);
    producer.Flush(*packet);
    Submit(std::move(packet));
    Complete(sequence);
    const auto *pixels = Pixels(sequence, viewport);
    ASSERT_TRUE(pixels);
    EXPECT_GT(pixels->bytes[(28 * 64 + 24) * 4 + 1], 240);
    EXPECT_LT(pixels->bytes[(28 * 64 + 24) * 4], 10);
    EXPECT_GT(pixels->bytes[(16 * 64 + 16) * 4], 240);
    EXPECT_GT(pixels->bytes[(28 * 64 + 40) * 4], 240);
    EXPECT_GT(pixels->bytes[(28 * 64 + 8) * 4], 240);
}

TEST_P(RendererRuntimeTest, SharedPassLayoutsKeepViewsAndQueuedFrameDataIndependent)
{
    auto             first = Scene();
    const ViewportId second{1, 1}, colorOutput{2, 1};
    ViewDesc         camera;
    camera.scene           = scene;
    camera.output          = second;
    camera.projection.v[0] = 0.25f;
    first->commands.push_back(CreateViewport{second, {ViewportKind::OFFSCREEN, {8, 8}}});
    first->commands.push_back(CreateViewport{colorOutput, {ViewportKind::OFFSCREEN, {8, 8}}});
    first->commands.push_back(CreateView{{1, 1}, camera});
    RenderEntityDesc entity;
    entity.mesh     = {Key(1201), 1};
    entity.material = {Key(1202), 1};
    first->commands.push_back(CreateRenderEntity{{scene, {0, 1}}, entity});
    first->commands.push_back(ResourceCpuReady{Mesh()});
    first->commands.push_back(ResourceCpuReady{HostMaterial(renderer->GetInfo(), true, true, false)});
    first->featureCommands.push_back(RegisterRenderFeature{{2, 1}, Name("header.color"), std::make_shared<int>(1), HeaderColor(renderer->GetInfo())});
    first->extensions.push_back(HeaderColorPayload(colorOutput, {0, 1, 0, 1}));
    first->readbacks = {viewport, second, colorOutput};
    Submit(std::move(first));
    Complete(sequence);
    ASSERT_TRUE(Until([&] {
        return std::count_if(events.begin(), events.end(), [&](const auto &event) {
                   const auto *resource = std::get_if<ResourceStateChanged>(&event);
                   return resource && resource->state == RenderResourceState::RESIDENT &&
                          (resource->resource == Key(1201) || resource->resource == Key(1202));
               }) == 2;
    }));
    auto baseline = Packet();
    baseline->extensions.push_back(HeaderColorPayload(colorOutput, {0, 1, 0, 1}));
    baseline->readbacks = {viewport, second, colorOutput};
    const auto initial  = baseline->sequence;
    Submit(std::move(baseline));
    entity.world.v[0] = 0.25f;
    auto next         = Packet({UpdateRenderEntity{{scene, {0, 1}}, entity, 1}});
    next->extensions.push_back(HeaderColorPayload(colorOutput, {0, 0, 1, 1}));
    next->readbacks = {viewport, second, colorOutput};
    Submit(std::move(next));
    Complete(sequence);
    ASSERT_TRUE(Until([&] { return Pixels(initial, viewport) && Pixels(sequence, colorOutput); }));
    Pixel(initial, viewport, 1, 4, {255, 0, 0});
    Pixel(initial, second, 1, 4, {0, 0, 0});
    Pixel(initial, second, 4, 4, {255, 0, 0});
    Pixel(initial, colorOutput, 1, 4, {0, 255, 0});
    Pixel(sequence, viewport, 1, 4, {0, 0, 0});
    Pixel(sequence, viewport, 4, 4, {255, 0, 0});
    Pixel(sequence, colorOutput, 1, 4, {0, 0, 255});
}

TEST_P(RendererRuntimeTest, SharedBatchShaderCanOmitViewAndUseStaticBatchBinding)
{
    auto  packet           = Scene();
    auto &camera           = std::get<CreateView>(packet->commands[2]).desc;
    camera.projection.v[0] = 0.25f;
    RenderEntityDesc entity;
    entity.mesh     = {Key(1201), 1};
    entity.material = {Key(1202), 1};
    packet->commands.push_back(CreateRenderEntity{{scene, {0, 1}}, entity});
    packet->commands.push_back(ResourceCpuReady{Mesh()});
    auto material = HostMaterial(renderer->GetInfo(), false, false, false);
    ASSERT_EQ(material.material->reflection.resources.size(), 1u);
    EXPECT_EQ(material.material->reflection.resources.front().set, 2u);
    EXPECT_EQ(material.material->reflection.resources.front().type, ShaderResourceType::UNIFORM_BUFFER);
    packet->commands.push_back(ResourceCpuReady{std::move(material)});
    Submit(std::move(packet));
    Complete(sequence);
    Pixel(sequence, viewport, 1, 4, {255, 0, 0});
}

TEST_P(RendererRuntimeTest, ScenePassRejectsWrongSharedLayoutAndDiscardsItsFrame)
{
    auto packet = Scene();
    AddMesh(*packet);
    auto &material = *std::get<ResourceCpuReady>(packet->commands.back()).payload.material;
    ASSERT_FALSE(material.reflection.blocks.empty());
    ASSERT_FALSE(material.reflection.blocks.front().members.empty());
    material.reflection.blocks.front().members.front().name = "wrongField";
    Submit(std::move(packet));
    ASSERT_TRUE(Until([&] { return Find<RendererFailure>(); }));
    EXPECT_NE(Find<RendererFailure>()->error.message.find("scene pass declarations"), std::string::npos);
    EXPECT_FALSE(Find<ViewportReadback>());
    events.clear();
    auto recover       = Packet({DestroyRenderEntity{{scene, {0, 1}}}});
    recover->readbacks = {viewport};
    Submit(std::move(recover));
    Complete(sequence);
    Pixel(sequence, viewport, 4, 4, {0, 0, 0});
}

TEST_P(RendererRuntimeTest, PassUniformRangeMismatchFailsBeforeGpuExecution)
{
    auto packet = Packet({CreateViewport{viewport, {ViewportKind::OFFSCREEN, {8, 8}}}});
    packet->featureCommands.push_back(
        RegisterRenderFeature{{0, 1}, Name("header.color"), std::make_shared<int>(1), HeaderColor(renderer->GetInfo(), true)});
    packet->extensions.push_back(HeaderColorPayload(viewport, {1, 0, 0, 1}));
    packet->readbacks = {viewport};
    Submit(std::move(packet));
    ASSERT_TRUE(Until([&] { return Find<RendererFailure>(); }));
    EXPECT_EQ(renderer->GetState(), RendererState::RUNNING);
    EXPECT_FALSE(Find<ViewportReadback>());
}

TEST_P(RendererRuntimeTest, MissingScenePassHasNoImplicitRasterFallback)
{
    auto packet = Scene();
    packet->featureCommands.clear();
    AddMesh(*packet);
    Submit(std::move(packet));
    ASSERT_TRUE(Until([&] { return Find<RendererFailure>(); }));
    EXPECT_NE(Find<RendererFailure>()->error.message.find("rendered RGBA8"), std::string::npos);
    EXPECT_FALSE(Find<ViewportReadback>());
    events.clear();
    auto next = Packet();
    ASSERT_FALSE(RegisterScenePass(*next, {63, 1}, std::make_shared<int>(1)));
    next->readbacks = {viewport};
    Submit(std::move(next));
    Complete(sequence);
    ASSERT_TRUE(Until([&] {
        return std::count_if(events.begin(), events.end(), [&](const auto &event) {
                   const auto *resource = std::get_if<ResourceStateChanged>(&event);
                   return resource && resource->state == RenderResourceState::RESIDENT &&
                          (resource->resource == Key(1201) || resource->resource == Key(1202));
               }) == 2;
    }));
    auto resident       = Packet();
    resident->readbacks = {viewport};
    Submit(std::move(resident));
    Complete(sequence);
    Pixel(sequence, viewport, 4, 4, {255, 0, 0});
}

TEST_P(RendererRuntimeTest, SharedTextureBindingsKeepProducerOutputsAndQueuedFramesIndependent)
{
    const ViewportId producer{0, 1}, firstOutput{1, 1}, secondOutput{2, 1};
    auto first = Packet({CreateViewport{producer, {ViewportKind::OFFSCREEN, {8, 8}}}, CreateViewport{firstOutput, {ViewportKind::OFFSCREEN, {8, 8}}},
                         CreateViewport{secondOutput, {ViewportKind::OFFSCREEN, {8, 8}}}});
    first->featureCommands.push_back(RegisterRenderFeature{{0, 1}, Name("header.color"), std::make_shared<int>(1), HeaderColor(renderer->GetInfo())});
    first->featureCommands.push_back(
        RegisterRenderFeature{{1, 1}, Name("sampled.color"), std::make_shared<int>(1), SampledColor(renderer->GetInfo())});
    first->extensions  = {HeaderColorPayload(producer, {1, 1, 1, 1}),
                          TextureToScreenInput(Name("sampled.color"), firstOutput, ViewOutputRef{producer, 0}, {1, 0, 0, 1}),
                          TextureToScreenInput(Name("sampled.color"), secondOutput, ViewOutputRef{producer, 0}, {0, 1, 0, 1})};
    first->readbacks   = {producer, firstOutput, secondOutput};
    const auto initial = first->sequence;
    Submit(std::move(first));
    auto next        = Packet();
    next->extensions = {HeaderColorPayload(producer, {0, 1, 1, 1}),
                        TextureToScreenInput(Name("sampled.color"), firstOutput, ViewOutputRef{producer, 0}, {0, 0, 1, 1}),
                        TextureToScreenInput(Name("sampled.color"), secondOutput, ViewOutputRef{producer, 0}, {1, 0, 0, 1})};
    next->readbacks  = {producer, firstOutput, secondOutput};
    Submit(std::move(next));
    Complete(sequence);
    Pixel(initial, producer, 4, 4, {255, 255, 255});
    Pixel(initial, firstOutput, 4, 4, {255, 0, 0});
    Pixel(initial, secondOutput, 4, 4, {0, 255, 0});
    Pixel(sequence, producer, 4, 4, {0, 255, 255});
    Pixel(sequence, firstOutput, 4, 4, {0, 0, 255});
    Pixel(sequence, secondOutput, 4, 4, {0, 0, 0});
}

TEST_P(RendererRuntimeTest, SharedTextureBindingsRejectStaleOutputThenRecoverAfterDiscard)
{
    const ViewportId sampled{1, 1};
    auto first = Packet({CreateViewport{viewport, {ViewportKind::OFFSCREEN, {8, 8}}}, CreateViewport{sampled, {ViewportKind::OFFSCREEN, {8, 8}}}});
    first->featureCommands.push_back(RegisterRenderFeature{{0, 1}, Name("header.color"), std::make_shared<int>(1), HeaderColor(renderer->GetInfo())});
    first->featureCommands.push_back(
        RegisterRenderFeature{{1, 1}, Name("sampled.color"), std::make_shared<int>(1), SampledColor(renderer->GetInfo())});
    first->extensions = {HeaderColorPayload(viewport, {1, 0, 0, 1}),
                         TextureToScreenInput(Name("sampled.color"), sampled, ViewOutputRef{viewport, 1})};
    first->readbacks  = {sampled};
    Submit(std::move(first));
    ASSERT_TRUE(Until([&] { return Find<RendererFailure>(); }));
    EXPECT_EQ(renderer->GetState(), RendererState::RUNNING);
    EXPECT_FALSE(Find<ViewportReadback>());
    events.clear();
    auto recover        = Packet({ResizeViewport{viewport, {16, 8}, 1}});
    recover->extensions = {HeaderColorPayload(viewport, {0, 1, 0, 1}),
                           TextureToScreenInput(Name("sampled.color"), sampled, ViewOutputRef{viewport, 1})};
    recover->readbacks  = {sampled};
    Submit(std::move(recover));
    Complete(sequence);
    Pixel(sequence, sampled, 4, 4, {0, 255, 0});
}

TEST_P(RendererRuntimeTest, SharedTextureBindingsPinExactAssetVersionsAcrossUploadAndRelease)
{
    const ViewportId other{1, 1};
    const auto       oldKey = Key(1401), newKey = Key(1401, 2);
    const auto       texture = [](RenderResourceKey key, std::vector<uint8_t> rgba) {
        RenderResourcePayload payload;
        payload.resource       = key;
        payload.format         = Name("pass.rgba8");
        payload.bytes          = std::move(rgba);
        payload.metadata.image = RenderImageLayout{1, 1};
        return ResourceCpuReady{std::move(payload)};
    };
    auto first = Packet({CreateViewport{viewport, {ViewportKind::OFFSCREEN, {8, 8}}}, CreateViewport{other, {ViewportKind::OFFSCREEN, {8, 8}}},
                         texture(oldKey, {255, 0, 0, 255})});
    first->featureCommands.push_back(
        RegisterRenderFeature{{0, 1}, Name("sampled.color"), std::make_shared<int>(1), SampledColor(renderer->GetInfo())});
    first->extensions  = {TextureToScreenInput(Name("sampled.color"), viewport, oldKey)};
    first->readbacks   = {viewport};
    const auto initial = first->sequence;
    Submit(std::move(first));
    Complete(initial);
    ASSERT_TRUE(Until([&] {
        return std::any_of(events.begin(), events.end(), [&](const auto &event) {
            const auto *resource = std::get_if<ResourceStateChanged>(&event);
            return resource && resource->resource == oldKey && resource->state == RenderResourceState::RESIDENT;
        });
    }));
    auto next        = Packet({texture(newKey, {0, 0, 255, 255})});
    next->extensions = {TextureToScreenInput(Name("sampled.color"), viewport, oldKey), TextureToScreenInput(Name("sampled.color"), other, newKey)};
    next->releasedResources = {oldKey};
    next->readbacks         = {viewport, other};
    Submit(std::move(next));
    Complete(sequence);
    Pixel(initial, viewport, 4, 4, {255, 0, 0});
    Pixel(sequence, viewport, 4, 4, {255, 0, 0});
    Pixel(sequence, other, 4, 4, {0, 0, 255});
}
