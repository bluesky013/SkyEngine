//
// Created on 2026/10/17.
//

#include "ShaderArtifactTestHelper.h"
#include <algorithm>
#include <array>
#include <aurora/adaptor/AuroraModule.h>
#include <aurora/adaptor/renderer/RenderSceneProxy.h>
#include <aurora/adaptor/renderer/RendererHost.h>
#include <aurora/adaptor/renderer/UiRenderFeature.h>
#include <aurora/shader/ShaderCacheStore.h>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <framework/application/ModuleManager.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/NativeWindowManager.h>
#include <fstream>
#include <gtest/gtest.h>
#include <span>
#include <string_view>

using namespace sky;
using namespace sky::aurora;

namespace {
    struct HostTrace {
        uint32_t creates = 0, polls = 0, stops = 0, destroys = 0;
    };
    class HostRenderer final : public IRenderer {
    public:
        explicit HostRenderer(HostTrace &trace) : trace(trace)
        {
        }
        ~HostRenderer() override
        {
            EXPECT_EQ(state, RendererState::STOPPED);
            ++trace.destroys;
        }
        PacketSubmitResult TrySubmit(std::unique_ptr<FramePacket> &) override
        {
            return {};
        }
        size_t PollEvents(std::span<RendererEvent> events) override
        {
            if (!events.empty() && state == RendererState::STOPPING && ++trace.polls == 2) {
                state     = RendererState::STOPPED;
                events[0] = RendererStopped{};
                return 1;
            }
            return 0;
        }
        RendererState GetState() const override
        {
            return state;
        }
        RendererInfo GetInfo() const override
        {
            return {API::VULKAN, 1};
        }
        void RequestShutdown() override
        {
            ++trace.stops;
            state = RendererState::STOPPING;
        }
        HostTrace    &trace;
        RendererState state = RendererState::RUNNING;
    };
    class HostProvider final : public IRendererProvider {
    public:
        RendererCreateResult Create(const RendererInitInfo &) override
        {
            ++trace.creates;
            if (fail)
                return RendererError{RendererErrorCode::OUT_OF_MEMORY, "injected creation failure"};
            return std::unique_ptr<IRenderer>(std::make_unique<HostRenderer>(trace));
        }
        HostTrace trace;
        bool      fail = false;
    };
} // namespace

TEST(RendererHost, PublishesOneOwnerAndDrainsBeforeUnregistering)
{
    HostProvider provider;
    RendererHost owner, duplicate;
    ASSERT_FALSE(owner.Init(provider, {}));
    EXPECT_EQ(Interface<IRenderer>::Get()->GetApi(), owner.Get());
    EXPECT_TRUE(duplicate.Init(provider, {}));
    EXPECT_EQ(provider.trace.creates, 1u);
    duplicate.Shutdown();
    EXPECT_EQ(Interface<IRenderer>::Get()->GetApi(), owner.Get());
    owner.Shutdown();
    EXPECT_EQ(provider.trace.polls, 2u);
    EXPECT_EQ(provider.trace.stops, 1u);
    EXPECT_EQ(provider.trace.destroys, 1u);
    EXPECT_EQ(Interface<IRenderer>::Get()->GetApi(), nullptr);
    owner.Shutdown();
    EXPECT_EQ(provider.trace.stops, 1u);
}

TEST(RendererHost, FailedCreationLeavesNoPublishedOrPartiallyOwnedRenderer)
{
    HostProvider provider;
    provider.fail = true;
    RendererHost host;
    const auto   error = host.Init(provider, {});
    ASSERT_TRUE(error);
    EXPECT_EQ(error->code, RendererErrorCode::OUT_OF_MEMORY);
    EXPECT_EQ(host.Get(), nullptr);
    EXPECT_EQ(Interface<IRenderer>::Get()->GetApi(), nullptr);
    host.Shutdown();
    EXPECT_EQ(provider.trace.stops, 0u);
    provider.fail = false;
    EXPECT_FALSE(host.Init(provider, {}));
}

TEST(RenderSceneProxy, RetirementAcknowledgementGuardsReuseAndDoesNotBorrowSceneStorage)
{
    RenderSceneProxy scene;
    FramePacket      create, destroy, next;
    EXPECT_TRUE(scene.Create(create, {}));
    EXPECT_TRUE(create.commands.empty());
    ASSERT_FALSE(scene.Create(create, {4, 1}, {Name("main")}));
    EXPECT_EQ(scene.GetSceneId(), (SceneId{4, 1}));
    EXPECT_TRUE(scene.Create(next, {5, 1}));
    ASSERT_FALSE(scene.Destroy(destroy));
    EXPECT_TRUE(scene.IsRetiring());
    EXPECT_FALSE(scene.GetSceneId().IsValid());
    EXPECT_TRUE(scene.Destroy(destroy));
    EXPECT_EQ(destroy.commands.size(), 1u);
    scene.OnEvent(SceneDestroyed{{4, 2}});
    EXPECT_TRUE(scene.IsRetiring());
    scene.OnEvent(SceneDestroyed{{4, 1}});
    EXPECT_FALSE(scene.IsRetiring());
    ASSERT_FALSE(scene.Create(next, {4, 2}));
    EXPECT_EQ(std::get<CreateScene>(create.commands.front()).id, (SceneId{4, 1}));
    EXPECT_EQ(std::get<DestroyScene>(destroy.commands.front()).id, (SceneId{4, 1}));
    scene.OnRendererStopped();
    EXPECT_FALSE(scene.GetSceneId().IsValid());
}

namespace {
    struct LauncherTrace {
        std::vector<std::unique_ptr<FramePacket>> packets;
        std::vector<RendererEvent>                events;
        std::function<void()>                     stopping;
        bool                                      busy = false;
    };
    class LauncherRenderer final : public IRenderer {
    public:
        explicit LauncherRenderer(std::shared_ptr<LauncherTrace> trace) : trace(std::move(trace))
        {
        }
        PacketSubmitResult TrySubmit(std::unique_ptr<FramePacket> &packet) override
        {
            if (trace->busy)
                return {PacketSubmitStatus::BUSY};
            trace->packets.push_back(std::move(packet));
            return {PacketSubmitStatus::ACCEPTED};
        }
        size_t PollEvents(std::span<RendererEvent> output) override
        {
            const auto count = std::min(output.size(), trace->events.size());
            std::move(trace->events.begin(), trace->events.begin() + count, output.begin());
            trace->events.erase(trace->events.begin(), trace->events.begin() + count);
            return count;
        }
        RendererState GetState() const override
        {
            return state;
        }
        RendererInfo GetInfo() const override
        {
            return {API::VULKAN, 1};
        }
        void RequestShutdown() override
        {
            if (state == RendererState::STOPPED)
                return;
            if (trace->stopping)
                trace->stopping();
            state = RendererState::STOPPED;
        }

    private:
        std::shared_ptr<LauncherTrace> trace;
        RendererState                  state = RendererState::RUNNING;
    };
    class LauncherProvider final : public IRendererProvider {
    public:
        explicit LauncherProvider(std::shared_ptr<LauncherTrace> trace) : trace(std::move(trace))
        {
        }
        RendererCreateResult Create(const RendererInitInfo &) override
        {
            return std::unique_ptr<IRenderer>(std::make_unique<LauncherRenderer>(trace));
        }

    private:
        std::shared_ptr<LauncherTrace> trace;
    };
    class LauncherWindow final : public NativeWindow {
    public:
        LauncherWindow()
        {
            SetID(712345);
            descriptor.width = descriptor.height = 64;
            NativeWindowManager::Get()->Register(this);
        }
        bool IsMainWindow() const override
        {
            return true;
        }
        std::shared_ptr<const void> RetainNativeHandle() const override
        {
            return surface;
        }
        void SetSurface(std::shared_ptr<const void> value)
        {
            surface   = std::move(value);
            winHandle = const_cast<void *>(surface.get());
            Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnSurfaceChanged, static_cast<const NativeWindow *>(this));
        }

    private:
        std::shared_ptr<const void> surface;
    };
    class LauncherSurfaceTest : public testing::Test, public ISystemNotify {
    protected:
        void SetUp() override
        {
            std::error_code error;
            root = std::filesystem::temp_directory_path(error) /
                   ("sky-launcher-surface-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            ASSERT_FALSE(error);
            std::filesystem::create_directories(root / "assets/shaders/fixture", error);
            ASSERT_FALSE(error);
            std::ofstream(root / "assets/shaders/shader-sources.json")
                << R"({"version":1,"modules":[{"owner":"fixture","dependencies":[],"includeRoots":[],"files":["fixture/Main.slang"]}]})";
            std::ofstream(root / "assets/shaders/fixture/Main.slang") << "[numthreads(1, 1, 1)] void main() {}";
            auto bundle   = FileSystemPtr(new NativeFileSystem(FilePath(root.string())));
            auto products = bundle->CreateSubSystem("products/shaders", true);
            ASSERT_TRUE(products);
            // SPIR-V 1.5 compute module: layout(local_size_x=1) in; void main() {}
            constexpr uint32_t spirv[] = {
                0x07230203, 0x00010500, 0x00000000, 0x00000005, 0x00000000, 0x00020011, 0x00000001, 0x0003000E, 0x00000000,
                0x00000001, 0x0005000F, 0x00000005, 0x00000001, 0x6E69616D, 0x00000000, 0x00060010, 0x00000001, 0x00000011,
                0x00000001, 0x00000001, 0x00000001, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00050036,
                0x00000002, 0x00000001, 0x00000000, 0x00000003, 0x000200F8, 0x00000004, 0x000100FD, 0x00010038,
            };
            ShaderReflection reflection;
            std::fill(std::begin(reflection.threadGroupSize), std::end(reflection.threadGroupSize), 1u);
            const auto native =
                test::MakeTestShaderArtifact(ShaderTarget::SPIRV, ShaderStageFlagBit::CS, {reinterpret_cast<const uint8_t *>(spirv), sizeof(spirv)},
                                             "main", std::move(reflection), "fixture/Main.slang");
            ASSERT_TRUE(native);
            ShaderCompileResult artifact;
            static_cast<ShaderStageArtifact &>(artifact) = *native;
            artifact.identity.schemaFp                   = HashSchema({});
            artifact.identity.variantHash                = HashShaderVariants({});
            std::string validation;
            ASSERT_TRUE(SealShaderArtifact(artifact, validation)) << validation;
            const auto &identity    = artifact.identity;
            const auto &environment = identity.environment;
            const char *platform    = nullptr;
            switch (environment.platform) {
            case ShaderPlatform::WINDOWS: platform = "windows"; break;
            case ShaderPlatform::MACOS: platform = "macos"; break;
            case ShaderPlatform::IOS: platform = "ios"; break;
            case ShaderPlatform::ANDROID_OS: platform = "android"; break;
            case ShaderPlatform::LINUX: platform = "linux"; break;
            default: break;
            }
            ASSERT_NE(platform, nullptr);
            std::ofstream(root / "products/shaders/distribution.json")
                << R"({"formatVersion":2,"platform":")" << platform << R"(","targets":["vulkan"],"environments":{"vulkan":{"target":0,"platform":)"
                << static_cast<uint32_t>(environment.platform) << R"(,"codeFormat":)" << static_cast<uint32_t>(environment.codeFormat)
                << R"(,"profile":")" << environment.profile << R"(","minimumVersion":)" << environment.minimumVersion << R"(,"requiredCapabilities":)"
                << environment.requiredCapabilities << R"(,"matrixLayout":)" << static_cast<uint32_t>(environment.matrixLayout)
                << R"(,"options":[]}}})";
            ShaderCacheKey key;
            key.sourcePath  = identity.sourcePath;
            key.sourceEntry = identity.sourceEntry;
            key.sourceHash  = identity.sourceHash;
            key.variantHash = identity.variantHash;
            key.schemaFp    = identity.schemaFp;
            key.entryHash   = HashEntryName(identity.sourceEntry);
            key.target      = static_cast<uint32_t>(environment.target);
            key.stage       = static_cast<uint32_t>(identity.stage);
            key.environment = environment;
            key.producer    = identity.producer;
            key.toolchainFp = ShaderProducerFingerprint(identity.producer);
            ShaderBlobStore blobs(products.Get(), products.Get());
            ASSERT_TRUE(blobs.Store(key, artifact)) << blobs.LastError();
            ShaderCacheIndex index;
            ASSERT_TRUE(UpsertArtifact(index, key.sourcePath, {}, key.schemaFp, key, artifact, {}, &validation)) << validation;
            ASSERT_TRUE(SaveIndex(*products, "vulkan", index));
            ShaderCompileResult stored;
            ASSERT_TRUE(blobs.Load(key, stored)) << blobs.LastError();
            ASSERT_EQ(stored.artifactId, artifact.artifactId);
            modules.AddStaticModule("Aurora.Render", std::make_unique<IModule>());
            modules.TopoSort();
            ASSERT_TRUE(modules.LoadModules({}));
            Interface<ISystemNotify>::Get()->Register(*this);
            window.SetSurface(std::make_shared<int>(1));
            module = std::make_unique<AuroraModule>(std::make_unique<LauncherProvider>(trace), std::move(bundle));
            ASSERT_TRUE(module->Init({}));
            module->Start();
        }
        void TearDown() override
        {
            module.reset();
            modules.UnLoadModules();
            Interface<ISystemNotify>::Get()->UnRegister();
            std::error_code error;
            std::filesystem::remove_all(root, error);
        }
        void SetExit() override
        {
            failed = true;
        }
        void *GetMainWindowHandle() const override
        {
            return window.GetNativeHandle();
        }
        ModuleManager *GetModuleManager() const override
        {
            return const_cast<ModuleManager *>(&modules);
        }
        template <typename T>
        std::vector<T> Commands() const
        {
            std::vector<T> result;
            for (const auto &packet : trace->packets)
                for (const auto &command : packet->commands)
                    if (const auto *value = std::get_if<T>(&command))
                        result.push_back(*value);
            return result;
        }
        std::filesystem::path          root;
        std::shared_ptr<LauncherTrace> trace = std::make_shared<LauncherTrace>();
        LauncherWindow                 window;
        ModuleManager                  modules;
        std::unique_ptr<AuroraModule>  module;
        bool                           failed = false;
    };
} // namespace

TEST_F(LauncherSurfaceTest, ReplacementWaitsForExactRetirementAndRebindsTheExistingSceneAndView)
{
    uint32_t sources = 0;
    module->SetSceneSource([&](FramePacket &, SceneId scene) {
        EXPECT_EQ(scene, (SceneId{0, 1}));
        ++sources;
    });
    std::weak_ptr<const void> old = window.RetainNativeHandle();
    module->Tick(0);
    trace->busy = true;
    window.SetSurface({});
    auto                      intermediate = std::make_shared<int>(2);
    std::weak_ptr<const void> unused       = intermediate;
    window.SetSurface(intermediate);
    intermediate.reset();
    auto latest = std::make_shared<int>(3);
    window.SetSurface(latest);
    EXPECT_TRUE(unused.expired());
    EXPECT_FALSE(old.expired());
    trace->busy = false;
    module->Tick(0);
    EXPECT_EQ(Commands<DestroyViewport>().size(), 1u);
    EXPECT_EQ(Commands<CreateViewport>().size(), 1u);
    const auto before = sources;
    module->Tick(0);
    EXPECT_GT(sources, before);
    trace->events.push_back(ViewportDestroyed{{0, 2}});
    module->Tick(0);
    EXPECT_EQ(Commands<CreateViewport>().size(), 1u);
    trace->events.push_back(ViewportDestroyed{{0, 1}});
    module->Tick(0);
    EXPECT_TRUE(old.expired());
    const auto created = Commands<CreateViewport>();
    ASSERT_EQ(created.size(), 2u);
    EXPECT_EQ(created.back().id, (ViewportId{0, 2}));
    EXPECT_EQ(created.back().desc.nativeWindow, reinterpret_cast<uintptr_t>(latest.get()));
    const auto rebound = Commands<UpdateView>();
    ASSERT_EQ(rebound.size(), 1u);
    EXPECT_EQ(rebound[0].id, (ViewId{0, 1}));
    EXPECT_EQ(rebound[0].desc.output, (ViewportId{0, 2}));
    EXPECT_EQ(Commands<CreateScene>().size(), 1u);
    EXPECT_EQ(Commands<CreateView>().size(), 1u);
    EXPECT_TRUE(Commands<DestroyScene>().empty());
    EXPECT_FALSE(failed);
}

TEST_F(LauncherSurfaceTest, PendingNativeBorrowStaysAliveThroughShutdownDrain)
{
    std::weak_ptr<const void> old = window.RetainNativeHandle();
    trace->busy                   = true;
    window.SetSurface({});
    EXPECT_FALSE(old.expired());
    trace->stopping = [&] { EXPECT_FALSE(old.expired()); };
    module->Shutdown();
    EXPECT_TRUE(old.expired());
}

TEST_F(LauncherSurfaceTest, CameraUpdatesDuringSurfaceLossAreCoalescedAndRebound)
{
    float camera = 1;
    module->SetSceneSource([&](FramePacket &packet, SceneId scene) {
        ViewDesc description;
        description.scene          = scene;
        description.output         = {0, 1};
        description.cameraPosition = {camera, 2, 3};
        packet.commands.push_back(UpdateView{{0, 1}, description, 1});
        packet.extensions.push_back({.feature = Name("fixture"), .outputs = {{0, 1}}});
    });
    module->Tick(0);
    window.SetSurface({});
    const auto updates = Commands<UpdateView>().size();
    camera             = 8;
    module->Tick(0);
    EXPECT_EQ(Commands<UpdateView>().size(), updates);
    trace->events.push_back(ViewportDestroyed{{0, 1}});
    module->Tick(0);
    window.SetSurface(std::make_shared<int>(2));
    const auto rebound = Commands<UpdateView>();
    ASSERT_GT(rebound.size(), updates);
    EXPECT_EQ(rebound.back().desc.output, (ViewportId{0, 2}));
    EXPECT_EQ(rebound.back().desc.cameraPosition.x, 8);
    EXPECT_GT(rebound.back().revision, rebound.front().revision);
    module->Tick(0);
    ASSERT_EQ(trace->packets.back()->extensions.size(), 1u);
    EXPECT_EQ(trace->packets.back()->extensions.front().outputs, (std::vector<ViewportId>{{0, 2}}));
    EXPECT_FALSE(failed);
}

TEST_F(LauncherSurfaceTest, CloseDuringBackpressureCannotBeRevivedByALaterSurface)
{
    trace->busy = true;
    module->OnWindowClose(&window);
    window.SetSurface(std::make_shared<int>(9));
    trace->busy = false;
    module->Tick(0);
    EXPECT_EQ(Commands<CreateViewport>().size(), 1u);
    EXPECT_EQ(Commands<DestroyViewport>().size(), 1u);
    EXPECT_EQ(Commands<DestroyView>().size(), 1u);
    EXPECT_EQ(Commands<DestroyScene>().size(), 1u);
    trace->events.push_back(ViewportDestroyed{{0, 1}});
    module->Tick(0);
    window.SetSurface(std::make_shared<int>(10));
    module->Tick(0);
    EXPECT_EQ(Commands<CreateViewport>().size(), 1u);
    EXPECT_FALSE(failed);
}

TEST_F(LauncherSurfaceTest, RetryableOutputFailureKeepsTheLauncherRunningUntilRendererFailure)
{
    module->Tick(0);
    trace->events.push_back(ViewportFailed{{0, 1}, {RendererErrorCode::NOT_READY, "retry output query"}, false});
    module->Tick(0);
    EXPECT_FALSE(failed);
    EXPECT_EQ(Interface<IRenderer>::Get()->GetApi()->GetState(), RendererState::RUNNING);
    EXPECT_TRUE(Commands<DestroyViewport>().empty());
    trace->events.push_back(RendererFailure{1, {RendererErrorCode::UNSUPPORTED, "checked presentation unavailable"}});
    module->Tick(0);
    EXPECT_TRUE(failed);
    EXPECT_EQ(Interface<IRenderer>::Get()->GetApi()->GetState(), RendererState::STOPPED);
}

TEST_F(LauncherSurfaceTest, SurfaceFailureRequestsReplacementWithoutStoppingTheLauncher)
{
    module->Tick(0);
    trace->events.push_back(ViewportFailed{{0, 1}, {RendererErrorCode::NOT_READY, "surface lost"}, true});
    module->Tick(0);
    EXPECT_EQ(Commands<DestroyViewport>().size(), 1u);
    EXPECT_FALSE(failed);
    trace->events.push_back(ViewportDestroyed{{0, 1}});
    module->Tick(0);
    trace->events.push_back(ViewportFailed{{0, 2}, {RendererErrorCode::UNSUPPORTED, "checked presentation unavailable"}, false});
    module->Tick(0);
    EXPECT_FALSE(failed);
    EXPECT_EQ(Interface<IRenderer>::Get()->GetApi()->GetState(), RendererState::RUNNING);
}

TEST_F(LauncherSurfaceTest, CloseFromSceneSourceDoesNotSubmitThePacketReentrantly)
{
    module->SetSceneSource([&](FramePacket &, SceneId) { module->OnWindowClose(&window); });
    module->Tick(0);
    EXPECT_EQ(Commands<CreateViewport>().size(), 1u);
    EXPECT_EQ(Commands<DestroyViewport>().size(), 1u);
    EXPECT_EQ(Commands<DestroyScene>().size(), 1u);
    EXPECT_FALSE(failed);
}

namespace {
    class UiBuildCapture final : public FrameGraphBuildContext {
    public:
        explicit UiBuildCapture(API backend)
        {
            inputs.lease.context.backend = backend;
            target.output.viewport       = {0, 1};
            target.output.extent         = {256, 128};
            target.color                 = {1, 0};
            if (backend == API::METAL || backend == API::DX12)
                target.logicalToClip.v[5] = -1;
        }
        const FrameBuildInputs &GetInputs() const override
        {
            return inputs;
        }
        std::optional<FrameRenderTarget> GetOutput(ViewportId id) const override
        {
            return id == target.output.viewport ? std::optional{target} : std::nullopt;
        }
        std::optional<GraphImageHandle> ResolveViewOutput(ViewOutputRef) const override
        {
            return {};
        }
        std::optional<GraphImageHandle> ResolveImage(const RenderResourceKey &) const override
        {
            return GraphImageHandle{1, 1};
        }
        std::variant<FrameError, ShaderPtr> CreateShader(const RenderTechniqueData &code) override
        {
            // This CPU capture varies coordinate conventions without creating a native shader.
            EXPECT_EQ(code.backend, API::METAL);
            return ShaderPtr(new Shader);
        }
        std::variant<FrameError, SamplerPtr> CreateSampler(const Sampler::Descriptor &) override
        {
            return SamplerPtr(new Sampler);
        }
        std::variant<FrameError, RasterBufferRange> Upload(std::span<const std::byte> bytes, BufferUsageFlags usage) override
        {
            if (usage.TestBit(BufferUsageFlagBit::VERTEX)) {
                vertices.resize(bytes.size() / sizeof(RenderUiVertex));
                std::memcpy(vertices.data(), bytes.data(), bytes.size());
            }
            return RasterBufferRange{{1, nextBuffer++}, 0, bytes.size()};
        }
        std::variant<FrameError, RasterUniformBinding> WriteUniform(uint32_t, uint32_t, std::span<const std::byte>, ShaderResourceType) override
        {
            ADD_FAILURE() << "UI pass has no uniform buffers";
            return FrameError{FrameErrorCode::UNSUPPORTED};
        }
        std::variant<FrameError, std::vector<ResolvedRasterDraw>> ResolveDraws(Name, Rect2D, PreparedDrawList) override
        {
            ADD_FAILURE() << "UI pass has no scene extraction";
            return FrameError{FrameErrorCode::UNSUPPORTED};
        }
        std::variant<FrameError, GraphImageHandle> CreateImage(const Image::Descriptor &) override
        {
            ADD_FAILURE() << "UI pass uses imported images";
            return FrameError{FrameErrorCode::UNSUPPORTED};
        }
        FrameOperationResult Raster(FrameRasterPass pass) override
        {
            rasters.push_back(std::move(pass));
            return FrameOperationComplete{};
        }
        FrameOperationResult Fullscreen(FrameRasterPass) override
        {
            ADD_FAILURE() << "UI pass uses indexed raster";
            return FrameError{FrameErrorCode::UNSUPPORTED};
        }
        FrameOperationResult Compute(FrameComputePass) override
        {
            ADD_FAILURE() << "UI pass has no compute work";
            return FrameError{FrameErrorCode::UNSUPPORTED};
        }
        FrameBuildInputs             inputs;
        FrameRenderTarget            target;
        std::vector<RenderUiVertex>  vertices;
        std::vector<FrameRasterPass> rasters;
        uint32_t                     nextBuffer = 0;
    };
} // namespace

TEST(UiRenderFeature, MetalDx12AndVulkanKeepTopAndBottomClipsInTheSamePhysicalRows)
{
    RenderUiSnapshot snapshot;
    snapshot.output   = {0, 1};
    snapshot.vertices = {{0, 0}, {1, 0}, {0, 1}};
    snapshot.indices  = {0, 1, 2};
    const RenderResourceKey white{Uuid::Create(), 1, 0, 1};
    for (const auto clip : {std::array{.125f, .125f, .75f, .375f}, std::array{.125f, .625f, .75f, .875f}})
        snapshot.draws.push_back({.firstIndex = 0, .indexCount = 3, .clip = clip, .image = white});
    const auto encoded = EncodeRenderUiSnapshot(Name("ui.clip"), snapshot);
    ASSERT_TRUE(std::holds_alternative<FrameExtensionPayload>(encoded));
    constexpr std::string_view     source = R"MSL(
        #include <metal_stdlib>
        using namespace metal;
        struct UiInput { float2 position [[attribute(0)]]; float2 uv [[attribute(1)]]; float4 color [[attribute(2)]]; };
        struct UiVarying { float4 position [[position]]; float2 uv; float4 color; };
        vertex UiVarying VSMain(UiInput input [[stage_in]]) {
            return {float4(input.position, 0, 1), input.uv, input.color};
        }
        fragment float4 FSMain(UiVarying input [[stage_in]], texture2d<float> image [[texture(0)]], sampler imageSampler [[sampler(1)]]) {
            return image.sample(imageSampler, input.uv) * input.color;
        }
    )MSL";
    const std::span<const uint8_t> bytes{reinterpret_cast<const uint8_t *>(source.data()), source.size()};
    ShaderResource                 image{"image", ShaderResourceType::SAMPLED_IMAGE, 0, 0, 1};
    image.texture          = {ShaderTextureDimension::D2, ShaderScalarType::FLOAT, 4, false, false};
    image.stages           = ShaderStageFlagBit::FS;
    image.declarationOwner = "adaptor-test/Ui.metal";
    ShaderResource sampler{"imageSampler", ShaderResourceType::SAMPLER, 0, 1, 1};
    sampler.sampler          = ShaderSamplerType::REGULAR;
    sampler.stages           = ShaderStageFlagBit::FS;
    sampler.declarationOwner = image.declarationOwner;
    ShaderReflection vertexReflection, fragmentReflection;
    vertexReflection.declarations = fragmentReflection.declarations = {image, sampler};
    fragmentReflection.resources                                    = {image, sampler};
    fragmentReflection.nativeBindings                               = {{0, 0, ShaderStageFlagBit::FS, 0}, {0, 1, ShaderStageFlagBit::FS, 1}};
    fragmentReflection.nativeBindings[0].category                   = ShaderNativeCategory::METAL_TEXTURE;
    fragmentReflection.nativeBindings[1].category                   = ShaderNativeCategory::METAL_SAMPLER;
    RenderTechniqueData program;
    program.backend = API::METAL;
    program.vertex =
        test::MakeTestShaderArtifact(ShaderTarget::MSL, ShaderStageFlagBit::VS, bytes, "VSMain", std::move(vertexReflection), image.declarationOwner);
    program.fragment = test::MakeTestShaderArtifact(ShaderTarget::MSL, ShaderStageFlagBit::FS, bytes, "FSMain", std::move(fragmentReflection),
                                                    image.declarationOwner);
    ASSERT_TRUE(program.vertex);
    ASSERT_TRUE(program.fragment);
    std::string error;
    ASSERT_TRUE(MergeShaderReflections(program.vertex->reflection, program.reflection, error)) << error;
    ASSERT_TRUE(MergeShaderReflections(program.fragment->reflection, program.reflection, error)) << error;
    for (const auto backend : {API::METAL, API::DX12, API::VULKAN}) {
        SCOPED_TRACE(static_cast<uint32_t>(backend));
        auto created = CreateUiRenderFeature(Name("ui.clip"), program);
        ASSERT_TRUE(std::holds_alternative<std::shared_ptr<IFrameGraphFeature>>(created));
        auto           feature = std::get<std::shared_ptr<IFrameGraphFeature>>(created);
        UiBuildCapture context(backend);
        const auto     result = feature->BuildFrame(context, std::get<FrameExtensionPayload>(encoded));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(result));
        ASSERT_EQ(context.rasters.size(), 1u);
        ASSERT_EQ(context.rasters.front().draws.size(), 2u);
        for (size_t draw = 0; draw < 2; ++draw) {
            const auto &scissor = context.rasters.front().draws[draw].scissor;
            EXPECT_EQ(scissor.offset.x, 32);
            EXPECT_EQ(scissor.offset.y, draw == 0 ? 16 : 80);
            EXPECT_EQ(scissor.extent.width, 160u);
            EXPECT_EQ(scissor.extent.height, 32u);
        }
        ASSERT_EQ(context.vertices.size(), 3u);
        EXPECT_EQ(context.vertices.front().y, backend == API::VULKAN ? -1.f : 1.f);
        feature->OnRetired();
    }
}
