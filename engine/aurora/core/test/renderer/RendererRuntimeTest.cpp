//
// Created on 2026/10/17.
//

#include "HostSceneFixture.h"
#include "frame/ViewportMapping.h"
#include "graph/FrameFeatureRegistry.h"
#include <D3D12Device.h>
#include <VulkanDevice.h>
#include <array>
#include <atomic>
#include <aurora/adaptor/renderer/UiRenderFeature.h>
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
