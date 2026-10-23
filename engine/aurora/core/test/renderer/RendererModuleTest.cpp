#include <array>
#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MaterialInstanceAsset.h>
#include <aurora/adaptor/renderer/RendererHost.h>
#include <aurora/adaptor/renderer/SceneAssetSource.h>
#include <aurora/pipeline/SceneHighlight.h>
#include <aurora/rdg/DeviceFrameContext.h>
#include <aurora/rdg/FrozenFrameGraph.h>
#include <aurora/renderer/IRenderFeature.h>
#include <aurora/renderer/IRenderer.h>
#include <aurora/rhi/Device.h>
#include <aurora/rhi/Instance.h>
#include <aurora/rhi/SwapChain.h>
#include <aurora/shader/IShaderCompiler.h>
#include <aurora/shader/ShaderTechnique.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <framework/application/ModuleManager.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetProductBundle.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/platform/PlatformBase.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/window/NativeWindow.h>
#include <gtest/gtest.h>
#include <map>
#include <thread>

#if defined(_WIN32)
    #include <windows.h>
#endif

using namespace sky;
using namespace sky::aurora;

#if defined(AURORA_RENDERER_STATIC_FIXTURE)
extern "C" IModule *sky_module_create_Aurora_Render_Contract();
#elif defined(AURORA_RENDERER_PACKAGED_STATIC)
extern "C" IModule *sky_module_create_Aurora_Render();
#endif

namespace {
    using ProviderService = Interface<IRendererProvider>;

    struct LoadedProvider {
        ModuleManager manager;
        explicit LoadedProvider(bool runtime = false)
        {
#if defined(AURORA_RENDERER_STATIC_FIXTURE)
            manager.AddStaticModule("Aurora.Render", std::unique_ptr<IModule>(sky_module_create_Aurora_Render_Contract()));
#elif defined(AURORA_RENDERER_PACKAGED_STATIC)
            manager.AddStaticModule("Aurora.Render", std::unique_ptr<IModule>(sky_module_create_Aurora_Render()));
#else
            manager.RegisterModule({"Aurora.Render", {}});
#endif
            manager.TopoSort();
            StartArguments args;
            if (runtime)
                args.args = {"RendererModuleTest", "--aurora-provider-only"};
            manager.LoadModules(args);
        }
        ~LoadedProvider()
        {
            manager.UnLoadModules();
        }
    };
} // namespace

TEST(RendererModule, RegistersInHostEnvironmentAndReloads)
{
    // Obtain the service in the host BEFORE dlopen: catches per-DLL singletons.
    auto *service = ProviderService::Get();
    ASSERT_EQ(service->GetApi(), nullptr);
    for (int iteration = 0; iteration < 2; ++iteration) {
        {
            LoadedProvider loaded;
            auto          *provider = service->GetApi();
            ASSERT_NE(provider, nullptr);
            auto result = provider->Create({});
            ASSERT_TRUE(std::holds_alternative<RendererError>(result));
            EXPECT_EQ(std::get<RendererError>(result).code, RendererErrorCode::NOT_READY);
            EXPECT_FALSE(std::get<RendererError>(result).message.empty());
        }
        EXPECT_EQ(service->GetApi(), nullptr);
    }
}

TEST(RendererModule, RejectsZeroCountsAndBudgets)
{
    LoadedProvider loaded;
    auto          *provider = ProviderService::Get()->GetApi();
    ASSERT_NE(provider, nullptr);
    for (auto member : {&RendererInitInfo::inflightFrames, &RendererInitInfo::maxPendingPackets, &RendererInitInfo::prepareWorkers,
                        &RendererInitInfo::encodeWorkers}) {
        RendererInitInfo info;
        info.*member = 0;
        auto result  = provider->Create(info);
        ASSERT_TRUE(std::holds_alternative<RendererError>(result));
        EXPECT_EQ(std::get<RendererError>(result).code, RendererErrorCode::INVALID_ARGUMENT);
    }
    for (auto member : {&RendererInitInfo::maxPendingResourceBytes, &RendererInitInfo::maxUploadBytesPerFrame}) {
        RendererInitInfo info;
        info.*member = 0;
        auto result  = provider->Create(info);
        ASSERT_TRUE(std::holds_alternative<RendererError>(result));
        EXPECT_EQ(std::get<RendererError>(result).code, RendererErrorCode::INVALID_ARGUMENT);
    }
}

TEST(RendererModule, RuntimeServiceDrainsBeforeUnloadAndCanReload)
{
    Instance::Destroy();
    Instance::Get();
    uint64_t previousEpoch = 0;
    for (uint32_t iteration = 0; iteration < 2; ++iteration) {
        LoadedProvider loaded(true);
        auto          *provider = ProviderService::Get()->GetApi();
        ASSERT_TRUE(provider);
        auto created = provider->Create({});
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<IRenderer>>(created));
        auto renderer = std::move(std::get<std::unique_ptr<IRenderer>>(created));
        EXPECT_GT(renderer->GetInfo().deviceEpoch, previousEpoch);
        previousEpoch    = renderer->GetInfo().deviceEpoch;
        auto packet      = std::make_unique<FramePacket>();
        packet->sequence = 1;
        packet->commands.push_back(CreateScene{{0, 1}, {}});
        ASSERT_EQ(renderer->TrySubmit(packet).status, PacketSubmitStatus::ACCEPTED);
        renderer->RequestShutdown();
        bool       stopped = false, applied = false, completed = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!stopped && std::chrono::steady_clock::now() < deadline) {
            std::array<RendererEvent, 8> events;
            const auto                   count = renderer->PollEvents(events);
            for (const auto &event : std::span(events).first(count)) {
                applied |= std::holds_alternative<PacketApplied>(event);
                completed |= std::holds_alternative<FrameCompleted>(event);
                stopped |= std::holds_alternative<RendererStopped>(event);
                EXPECT_FALSE(std::holds_alternative<RendererFailure>(event));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        EXPECT_TRUE(stopped && applied && completed);
        renderer.reset();
    }
}

namespace {
    class ShippedPasses final : public IFrameGraphFeature {
    public:
        ShippedPasses(RenderTechniqueData compute, RenderTechniqueData fullscreen) : compute(std::move(compute)), fullscreen(std::move(fullscreen))
        {
        }
        Name GetName() const override
        {
            return Name("package.passes");
        }
        FrameOperationResult BuildFrame(FrameGraphBuildContext &context, const FrameExtensionPayload &payload) override
        {
            if (payload.outputs.size() != 1)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Expected one package output"};
            const auto output = context.GetOutput(payload.outputs.front());
            if (!output)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Package output is unavailable"};
            const auto dispatchShader = context.CreateShader(compute);
            if (const auto *error = std::get_if<FrameError>(&dispatchShader))
                return *error;
            const auto rasterShader = context.CreateShader(fullscreen);
            if (const auto *error = std::get_if<FrameError>(&rasterShader))
                return *error;
            Image::Descriptor descriptor;
            descriptor.extent = {output->output.extent.width, output->output.extent.height, 1};
            descriptor.format = PixelFormat::RGBA8_UNORM;
            descriptor.usage  = ImageUsageFlagBit::STORAGE | ImageUsageFlagBit::SAMPLED;
            const auto image  = context.CreateImage(descriptor);
            if (const auto *error = std::get_if<FrameError>(&image))
                return *error;
            const auto       target = std::get<GraphImageHandle>(image);
            FrameComputePass dispatch;
            dispatch.shader  = std::get<ShaderPtr>(dispatchShader);
            dispatch.groupsX = descriptor.extent.width;
            dispatch.groupsY = descriptor.extent.height;
            dispatch.images.push_back({1, 0, target, {}, FrameAccessMode::WRITE});
            const auto executed = context.Compute(std::move(dispatch));
            if (std::holds_alternative<FrameError>(executed))
                return executed;
            const auto sampler = context.CreateSampler({});
            if (const auto *error = std::get_if<FrameError>(&sampler))
                return *error;
            RasterDraw draw;
            draw.shader              = std::get<ShaderPtr>(rasterShader);
            draw.command.vertexCount = 3;
            draw.scissor.extent      = output->output.extent;
            draw.images.push_back({1, 0, target});
            draw.samplers.push_back({1, 1, std::get<SamplerPtr>(sampler)});
            FrameRasterPass pass;
            pass.color = output->color;
            pass.draws.push_back(std::move(draw));
            return context.Fullscreen(std::move(pass));
        }

    private:
        RenderTechniqueData compute, fullscreen;
    };

    void VerifyShippedProducts(bool explicitPasses)
    {
        const char *root = std::getenv("SKY_SHADER_PACKAGE_ROOT");
        if (!root || !*root)
            GTEST_SKIP() << "Run python/tests/run_shader_package_tests.py to supply stripped products";
        ASSERT_EQ(ShaderCompilerFactory::Get().GetCompiler(), nullptr);
        LoadedProvider loaded(true);
        auto          *provider = ProviderService::Get()->GetApi();
        ASSERT_TRUE(provider);
        RendererInitInfo info;
        if (const char *backend = std::getenv("SKY_SHADER_PACKAGE_BACKEND")) {
            const std::string name(backend);
            ASSERT_TRUE(name == "metal" || name == "vulkan" || name == "d3d12");
            info.backend = name == "metal" ? API::METAL : name == "vulkan" ? API::VULKAN : API::DX12;
        }
        RendererHost host;
        const auto   initialized = host.Init(*provider, info);
        ASSERT_FALSE(initialized) << initialized->message;
        auto *renderer = host.Get();
        ASSERT_TRUE(renderer);
        FileSystemPtr products(new NativeFileSystem(FilePath(root)));
        ASSERT_TRUE(products->FileExist("requirements.json"));
        auto sources = std::make_shared<ShaderFileSystem>();
        auto lease   = loaded.manager.RetainModule("Aurora.Render");
        ASSERT_TRUE(lease);
        constexpr SceneId    scene{0, 1};
        constexpr ViewportId output{0, 1};
        auto                 packet = std::make_unique<FramePacket>();
        packet->sequence            = 1;
        packet->commands.push_back(CreateViewport{output, {ViewportKind::OFFSCREEN, {8, 8}}});
        if (explicitPasses) {
            ShaderTechnique compute;
            compute.compute     = {"first/pipeline/PackageCompute.slang", "FillTarget"};
            const auto dispatch = CompileShaderTechnique(*sources, renderer->GetInfo().backend, compute, products.Get());
            ASSERT_TRUE(std::holds_alternative<RenderTechniqueData>(dispatch)) << std::get<RendererError>(dispatch).message;
            const auto fullscreen = CompileShaderTechnique(
                *sources, renderer->GetInfo().backend,
                {{"first/pipeline/PackageFullscreen.slang", "DrawQuad"}, {"first/pipeline/PackageFullscreen.slang", "SampleTarget"}}, products.Get());
            ASSERT_TRUE(std::holds_alternative<RenderTechniqueData>(fullscreen)) << std::get<RendererError>(fullscreen).message;
            packet->featureCommands.push_back(RegisterRenderFeature{
                {0, 1},
                Name("package.passes"),
                lease,
                std::make_shared<ShippedPasses>(std::get<RenderTechniqueData>(dispatch), std::get<RenderTechniqueData>(fullscreen))});
            packet->extensions.push_back({Name("package.passes"), {}, {output}, {}});
        } else {
            const auto source   = CreateSceneAssetSource(sources, {}, products);
            const auto resolved = source->ResolveMesh(ScenePrimitiveId(ScenePrimitive::CUBE), {}, renderer->GetInfo());
            ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(resolved));
            const auto resources = std::get<std::shared_ptr<const SceneMeshResources>>(resolved);
            ASSERT_TRUE(resources);
            ASSERT_FALSE(resources->parts.empty());
            packet->commands.push_back(CreateScene{scene, {}});
            ViewDesc camera;
            camera.scene  = scene;
            camera.output = output;
            packet->commands.push_back(CreateView{{0, 1}, camera});
            ASSERT_FALSE(RegisterScenePass(*packet, {0, 1}, lease, sources, products));
            RenderEntityDesc entity;
            entity.mesh        = resources->parts.front().mesh;
            entity.material    = resources->parts.front().material;
            entity.world.v[14] = .5f;
            packet->commands.push_back(CreateRenderEntity{{scene, {0, 1}}, entity});
            for (const auto &payload : resources->payloads)
                packet->commands.push_back(ResourceCpuReady{payload});
        }
        packet->readbacks.push_back(output);
        ASSERT_EQ(renderer->TrySubmit(packet).status, PacketSubmitStatus::ACCEPTED);
        bool                            complete = false;
        std::optional<ViewportReadback> pixels;
        const auto                      deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while ((!complete || !pixels) && std::chrono::steady_clock::now() < deadline) {
            std::array<RendererEvent, 32> events;
            const auto                    count = renderer->PollEvents(events);
            for (auto &event : std::span(events).first(count)) {
                if (const auto *failure = std::get_if<RendererFailure>(&event))
                    FAIL() << failure->error.message;
                if (const auto *failure = std::get_if<ViewportFailed>(&event))
                    FAIL() << failure->error.message;
                if (const auto *failure = std::get_if<ResourceStateChanged>(&event); failure && failure->state == RenderResourceState::FAILED)
                    FAIL() << "Shipping resource " << failure->resource.product.ToString() << " revision " << failure->resource.revision << " epoch "
                           << failure->resource.deviceEpoch << " failed: " << failure->error.message;
                if (const auto *failure = std::get_if<ScenePrepareFailed>(&event))
                    FAIL() << "Shipping scene " << failure->scene.index << ':' << failure->scene.generation << " packet " << failure->sequence
                           << " failed: " << failure->error.message;
                if (const auto *finished = std::get_if<FrameCompleted>(&event))
                    complete |= finished->sequence == 1 && finished->submissionSerial != 0;
                if (auto *read = std::get_if<ViewportReadback>(&event); read && read->sequence == 1 && read->viewport == output)
                    pixels = std::move(*read);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        ASSERT_TRUE(complete);
        ASSERT_TRUE(pixels);
        ASSERT_EQ(pixels->extent.width, 8u);
        ASSERT_EQ(pixels->extent.height, 8u);
        ASSERT_EQ(pixels->bytes.size(), 8u * 8u * 4u);
        const auto expected = explicitPasses ? std::array{0, 0, 255} : std::array{166, 184, 209};
        const auto offset   = (4 * 8 + 4) * 4;
        for (size_t channel = 0; channel < expected.size(); ++channel)
            EXPECT_NEAR(pixels->bytes[offset + channel], expected[channel], 2);
        EXPECT_EQ(ShaderCompilerFactory::Get().GetCompiler(), nullptr);
        host.Shutdown();
    }
} // namespace

TEST(RendererModule, ShippedProductsRenderSceneWithoutCompiler)
{
    VerifyShippedProducts(false);
}

TEST(RendererModule, ShippedProductsComputeAndFullscreenWithoutCompiler)
{
    VerifyShippedProducts(true);
}

#if defined(SKY_PLATFORM_MACOS)
namespace {
    struct CompilerModule {
        ModuleManager manager;
        explicit CompilerModule(bool enabled)
        {
            if (!enabled)
                return;
            manager.RegisterModule({"Aurora.ShaderCompiler", {}});
            manager.TopoSort();
            manager.LoadModules({});
        }
        ~CompilerModule()
        {
            manager.UnLoadModules();
        }
    };

    void VerifyNamedMaterialAssets(bool shipped)
    {
        CompilerModule compiler(!shipped);
        ASSERT_EQ(ShaderCompilerFactory::Get().GetCompiler() != nullptr, !shipped);
        FileSystemPtr shaderProducts;
        if (shipped) {
            const auto *root = std::getenv("SKY_SHADER_PACKAGE_ROOT");
            if (!root || !*root)
                GTEST_SKIP() << "Run python/tests/run_shader_package_tests.py to supply stripped products";
            shaderProducts = new NativeFileSystem(FilePath(root));
        }
        LoadedProvider loaded(true);
        auto          *provider = ProviderService::Get()->GetApi();
        ASSERT_TRUE(provider);
        RendererHost host;
        const auto   initialized = host.Init(*provider, {.backend = API::METAL});
        ASSERT_FALSE(initialized) << initialized->message;
        auto *renderer = host.Get();
        ASSERT_TRUE(renderer);
        auto             sources = std::make_shared<ShaderFileSystem>();
        NativeFileSystem shaderRoot{FilePath("assets/shaders")};
        std::string      error;
        if (!shipped) {
            ASSERT_TRUE(sources->LoadBundles(shaderRoot, "", &error)) << error;
            sources->AddVirtualFile("material/NamedSample.slang", R"(
        #include <aurora/pipeline/SceneBatch.slangh>
        struct MaterialValues {
            [AuroraField("tint")] float4 tint;
            [AuroraField("gain")] float4 gain;
        };
        [AuroraResource(2, 2, "fixture/material")] ConstantBuffer<MaterialValues> materialValues;
        [AuroraResource(2, 3, "fixture/material")] Texture2D<float4> materialColor;
        [AuroraResource(2, 4, "fixture/material")] SamplerState materialSampler;
        struct Vertex { float4 position : SV_Position; float2 uv : TEXCOORD0; };
        Vertex VSMain(float3 position : POSITION) {
            Vertex v;
            v.position = mul(gSceneBatch.world, float4(position.xy * 1.8, 0.5, 1));
            v.uv = position.xy + 0.5;
            return v;
        }
        float4 FSMain(Vertex v) : SV_Target {
            return materialColor.Sample(materialSampler, v.uv) * materialValues.tint * materialValues.gain;
        }
    )");
            sources->AddVirtualFile("material/NamedSample.slang.json", R"({"entries":["VSMain","FSMain"],"variants":[]})");
        }
        const auto directory = std::filesystem::temp_directory_path() / ("sky_material_native_" + Uuid::Create().ToString());
        std::filesystem::create_directories(directory);
        struct Cleanup {
            std::filesystem::path path;
            ~Cleanup()
            {
                std::error_code ignored;
                std::filesystem::remove_all(path, ignored);
            }
        } cleanup{directory};
        NativeFileSystemPtr products = new NativeFileSystem(FilePath(directory));
        const std::string   bundle   = "native-material-" + Uuid::Create().ToString();
        auto               *assets   = AssetManager::Get();
        assets->RegisterAssetHandler<Material>();
        assets->RegisterAssetHandler<MaterialInstance>();
        assets->RegisterAssetHandler<Texture>();
        assets->AddAssetProductBundle(new HashedAssetBundle(products, bundle));
        auto material = assets->FindOrCreateAsset<Material>(Uuid::Create());
        auto instance = assets->FindOrCreateAsset<MaterialInstance>(Uuid::Create());
        auto imageA   = assets->FindOrCreateAsset<Texture>(Uuid::Create());
        auto imageB   = assets->FindOrCreateAsset<Texture>(Uuid::Create());
        ASSERT_TRUE(material && instance && imageA && imageB);
        const auto imageData = [](std::array<uint8_t, 4> rgba) {
            ImageAssetData data;
            data.format   = PixelFormat::RGBA8_SRGB;
            data.dataSize = 4;
            data.slices   = {{0, 4, 0}};
            data.rawData.assign(rgba.begin(), rgba.end());
            return data;
        };
        imageA->Data() = imageData({128, 128, 128, 255});
        imageB->Data() = imageData({255, 128, 64, 255});
        imageA->AddBulk(imageA->Data().rawData.data(), imageA->Data().rawData.size());
        imageB->AddBulk(imageB->Data().rawData.data(), imageB->Data().rawData.size());
        MaterialTechniqueData technique;
        technique.tag                     = "opaque";
        const std::string materialProgram = shipped ? "first/pipeline/PackageMaterial.slang" : "material/NamedSample.slang";
        technique.program                 = {{materialProgram, "VSMain"}, {materialProgram, "FSMain"}};
        technique.aliases                 = {{"baseTint", "materialValues.tint"},
                                             {"inheritedGain", "materialValues.gain"},
                                             {"baseColor", "materialColor"},
                                             {"colorSampler", "materialSampler"}};
        material->Data().techniques       = {technique};
        material->Data().properties       = {{"baseTint", MaterialPropertyType::FLOAT4, Vector4{.5f, 1, .5f, 1}},
                                             {"inheritedGain", MaterialPropertyType::FLOAT4, Vector4{1, .5f, 1, 1}},
                                             {"baseColor", MaterialPropertyType::TEXTURE2D, imageA->GetUuid()},
                                             {"colorSampler", MaterialPropertyType::SAMPLER, Sampler::Descriptor{}}};
        instance->Data().material         = material->GetUuid();
        instance->Data().valueOverrides   = {{"baseTint", MaterialPropertyType::FLOAT4, Vector4{.25f, 1, 1, 1}}};
        instance->Data().textureOverrides = {{"baseColor", imageB->GetUuid()}};
        material->AddDependencies(imageA->GetUuid());
        instance->AddDependencies(material->GetUuid());
        instance->AddDependencies(imageB->GetUuid());
        assets->SaveAsset(material, bundle);
        assets->SaveAsset(instance, bundle);
        assets->SaveAsset(imageA, bundle);
        ASSERT_TRUE(material->IsLoaded() && instance->IsLoaded() && imageA->IsLoaded());
        auto       source = CreateSceneAssetSource(sources, [assets](const Uuid &id) { return assets->FindAsset(id); }, shaderProducts);
        const auto mesh   = ScenePrimitiveId(ScenePrimitive::CUBE);
        EXPECT_TRUE(std::holds_alternative<std::monostate>(source->ResolveMesh(mesh, instance->GetUuid(), renderer->GetInfo())));
        assets->SaveAsset(imageB, bundle);
        ASSERT_TRUE(imageB->IsLoaded());
        const auto materialId = material->GetUuid(), instanceId = instance->GetUuid();
        const auto imageAId = imageA->GetUuid(), imageBId = imageB->GetUuid();
        source.reset();
        material.reset();
        instance.reset();
        imageA.reset();
        imageB.reset();
        material = assets->LoadAsset<Material>(materialId);
        instance = assets->LoadAsset<MaterialInstance>(instanceId);
        imageA   = assets->LoadAsset<Texture>(imageAId);
        imageB   = assets->LoadAsset<Texture>(imageBId);
        ASSERT_TRUE(material && instance && imageA && imageB);
        for (const AssetPtr &asset : {AssetPtr(material), AssetPtr(instance), AssetPtr(imageA), AssetPtr(imageB)}) {
            asset->BlockUntilLoaded();
            ASSERT_TRUE(asset->IsLoaded());
        }
        ASSERT_TRUE(imageA->HasBulk() && imageB->HasBulk());
        EXPECT_TRUE(imageA->Data().rawData.empty() && imageB->Data().rawData.empty());
        source                    = CreateSceneAssetSource(sources, [assets](const Uuid &id) { return assets->FindAsset(id); }, shaderProducts);
        const auto baseResult     = source->ResolveMesh(mesh, material->GetUuid(), renderer->GetInfo());
        const auto instanceResult = source->ResolveMesh(mesh, instance->GetUuid(), renderer->GetInfo());
        ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(baseResult));
        ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(instanceResult));
        const auto base       = std::get<std::shared_ptr<const SceneMeshResources>>(baseResult);
        const auto overridden = std::get<std::shared_ptr<const SceneMeshResources>>(instanceResult);
        ASSERT_EQ(base->parts.size(), 1u);
        ASSERT_EQ(overridden->parts.size(), 1u);
        const auto findPayload = [](const SceneMeshResources &resources, const Uuid &id) -> const RenderResourcePayload * {
            const auto found = std::find_if(resources.payloads.begin(), resources.payloads.end(),
                                            [&](const auto &payload) { return payload.resource.product == id; });
            return found == resources.payloads.end() ? nullptr : &*found;
        };
        const auto *oldImage    = findPayload(*overridden, imageB->GetUuid());
        const auto *oldMaterial = findPayload(*overridden, instance->GetUuid());
        ASSERT_TRUE(oldImage && oldMaterial && oldMaterial->material);
        ASSERT_TRUE(oldImage->metadata.image);
        EXPECT_EQ(oldImage->metadata.image->format, RenderImageFormat::RGBA8_SRGB);
        const auto &parameters = oldMaterial->material->techniques.front().parameters;
        ASSERT_EQ(parameters.values.size(), 2u);
        ASSERT_EQ(parameters.images.size(), 1u);
        EXPECT_EQ(parameters.images.front().name, Name("baseColor"));
        EXPECT_EQ(parameters.images.front().resource, oldImage->resource);
        constexpr SceneId                   scene{0, 1};
        constexpr std::array<ViewportId, 3> outputs{{{0, 1}, {1, 1}, {2, 1}}};
        const auto                          addView = [&](FramePacket &packet, uint32_t index, const RenderResourceBinding &binding) {
            packet.commands.push_back(CreateViewport{outputs[index], {ViewportKind::OFFSCREEN, {8, 8}}});
            ViewDesc view;
            view.scene          = scene;
            view.output         = outputs[index];
            view.visibilityMask = uint64_t{1} << index;
            packet.commands.push_back(CreateView{{index, 1}, view});
            RenderEntityDesc entity;
            entity.mesh           = base->parts.front().mesh;
            entity.material       = binding;
            entity.visibilityMask = view.visibilityMask;
            packet.commands.push_back(CreateRenderEntity{{scene, {index, 1}}, entity});
        };
        const auto finish = [&](std::unique_ptr<FramePacket> packet, std::span<const ViewportId> requested,
                                std::map<uint32_t, ViewportReadback> &pixels) -> bool {
            const auto sequence = packet->sequence;
            packet->readbacks.assign(requested.begin(), requested.end());
            const auto submitted = renderer->TrySubmit(packet);
            if (submitted.status != PacketSubmitStatus::ACCEPTED) {
                ADD_FAILURE() << "Packet " << sequence << " was not accepted: " << submitted.error.message;
                return false;
            }
            bool       complete = false;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while ((!complete || pixels.size() != requested.size()) && std::chrono::steady_clock::now() < deadline) {
                std::array<RendererEvent, 64> events;
                const auto                    count = renderer->PollEvents(events);
                for (auto &event : std::span(events).first(count)) {
                    if (const auto *failure = std::get_if<RendererFailure>(&event)) {
                        ADD_FAILURE() << failure->error.message;
                        return false;
                    }
                    if (const auto *failure = std::get_if<ViewportFailed>(&event)) {
                        ADD_FAILURE() << failure->error.message;
                        return false;
                    }
                    if (const auto *failure = std::get_if<ScenePrepareFailed>(&event)) {
                        ADD_FAILURE() << failure->error.message;
                        return false;
                    }
                    if (const auto *failure = std::get_if<ResourceStateChanged>(&event); failure && failure->state == RenderResourceState::FAILED) {
                        ADD_FAILURE() << failure->error.message;
                        return false;
                    }
                    if (const auto *finished = std::get_if<FrameCompleted>(&event); finished && finished->sequence == sequence)
                        complete = finished->submissionSerial != 0;
                    if (auto *read = std::get_if<ViewportReadback>(&event); read && read->sequence == sequence)
                        pixels.emplace(read->viewport.index, std::move(*read));
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return complete && pixels.size() == requested.size();
        };
        const auto pixel = [](const ViewportReadback &read, std::array<float, 3> expected) {
            ASSERT_EQ(read.extent.width, 8u);
            ASSERT_EQ(read.extent.height, 8u);
            ASSERT_EQ(read.bytes.size(), 8u * 8u * 4u);
            const auto offset = (4 * 8 + 4) * 4;
            for (size_t i = 0; i < expected.size(); ++i)
                EXPECT_NEAR(read.bytes[offset + i], expected[i], 2) << "channel " << i;
        };
        const auto linear = [](float encoded) {
            encoded /= 255;
            return (encoded <= .04045f ? encoded / 12.92f : std::pow((encoded + .055f) / 1.055f, 2.4f)) * 255;
        };
        const std::array baseColor{linear(128) * .5f, linear(128) * .5f, linear(128) * .5f};
        const std::array oldColor{255 * .25f, linear(128) * .5f, linear(64)};
        auto             first = std::make_unique<FramePacket>();
        first->sequence        = 1;
        first->commands.push_back(CreateScene{scene, {}});
        addView(*first, 0, base->parts.front().material);
        addView(*first, 1, overridden->parts.front().material);
        auto moduleLease = loaded.manager.RetainModule("Aurora.Render");
        ASSERT_TRUE(moduleLease);
        ASSERT_FALSE(RegisterScenePass(*first, {0, 1}, moduleLease, sources, shaderProducts));
        std::vector<RenderResourceKey> supplied;
        for (const auto *resources : {base.get(), overridden.get()})
            for (const auto &payload : resources->payloads)
                if (payload.resource != oldImage->resource && std::find(supplied.begin(), supplied.end(), payload.resource) == supplied.end()) {
                    first->commands.push_back(ResourceCpuReady{payload});
                    supplied.push_back(payload.resource);
                }
        std::map<uint32_t, ViewportReadback> pixels;
        ASSERT_TRUE(finish(std::move(first), std::span(outputs).first(2), pixels));
        pixel(pixels.at(0), baseColor);
        pixel(pixels.at(1), {0, 0, 0});
        auto second      = std::make_unique<FramePacket>();
        second->sequence = 2;
        second->commands.push_back(ResourceCpuReady{*oldImage});
        pixels.clear();
        ASSERT_TRUE(finish(std::move(second), std::span(outputs).first(2), pixels));
        pixel(pixels.at(0), baseColor);
        pixel(pixels.at(1), oldColor);
        auto newImage                                                               = *oldImage;
        newImage.resource.revision                                                  = 2;
        newImage.bytes                                                              = {64, 255, 128, 255};
        auto newMaterial                                                            = *oldMaterial;
        newMaterial.resource.revision                                               = 2;
        newMaterial.material->techniques.front().parameters.images.front().resource = newImage.resource;
        auto third                                                                  = std::make_unique<FramePacket>();
        third->sequence                                                             = 3;
        third->commands.push_back(ResourceCpuReady{newMaterial});
        addView(*third, 2, {newMaterial.resource, 1});
        third->releasedResources.push_back(oldImage->resource);
        pixels.clear();
        ASSERT_TRUE(finish(std::move(third), outputs, pixels));
        pixel(pixels.at(0), baseColor);
        pixel(pixels.at(1), oldColor);
        pixel(pixels.at(2), {0, 0, 0});
        auto fourth      = std::make_unique<FramePacket>();
        fourth->sequence = 4;
        fourth->commands.push_back(ResourceCpuReady{newImage});
        pixels.clear();
        ASSERT_TRUE(finish(std::move(fourth), outputs, pixels));
        pixel(pixels.at(0), baseColor);
        pixel(pixels.at(1), oldColor);
        pixel(pixels.at(2), {linear(64) * .25f, 255 * .5f, linear(128)});
        EXPECT_EQ(ShaderCompilerFactory::Get().GetCompiler() != nullptr, !shipped);
        host.Shutdown();
    }
} // namespace

TEST(RendererModule, MetalSelectionOutlineUsesTypedParametersAndExactEntityGeneration)
{
    CompilerModule compiler(true);
    ASSERT_TRUE(ShaderCompilerFactory::Get().GetCompiler());
    LoadedProvider loaded(true);
    auto          *provider = ProviderService::Get()->GetApi();
    ASSERT_TRUE(provider);
    RendererHost host;
    const auto   initialized = host.Init(*provider, {.backend = API::METAL});
    ASSERT_FALSE(initialized) << initialized->message;
    auto *renderer = host.Get();
    ASSERT_TRUE(renderer);
    auto             sources = std::make_shared<ShaderFileSystem>();
    NativeFileSystem shaderRoot{FilePath("assets/shaders")};
    std::string      error;
    ASSERT_TRUE(sources->LoadBundles(shaderRoot, "", &error)) << error;
    const auto source   = CreateSceneAssetSource(sources);
    const auto resolved = source->ResolveMesh(ScenePrimitiveId(ScenePrimitive::CUBE), {}, renderer->GetInfo());
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(resolved));
    const auto resources = std::get<std::shared_ptr<const SceneMeshResources>>(resolved);
    ASSERT_TRUE(resources);
    ASSERT_EQ(resources->parts.size(), 1u);
    const auto moduleLease = loaded.manager.RetainModule("Aurora.Render");
    ASSERT_TRUE(moduleLease);
    constexpr SceneId         scene{0, 1};
    constexpr ViewportId      output{0, 1};
    constexpr RenderEntityRef entity{scene, {0, 1}};
    const auto                finish = [&](std::unique_ptr<FramePacket> packet, std::vector<uint8_t> &pixels) -> bool {
        const auto sequence = packet->sequence;
        packet->readbacks.push_back(output);
        const auto submitted = renderer->TrySubmit(packet);
        if (submitted.status != PacketSubmitStatus::ACCEPTED) {
            ADD_FAILURE() << "Packet " << sequence << " was not accepted: " << submitted.error.message;
            return false;
        }
        bool       complete = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        while ((!complete || pixels.empty()) && std::chrono::steady_clock::now() < deadline) {
            std::array<RendererEvent, 32> events;
            const auto                    count = renderer->PollEvents(events);
            for (auto &event : std::span(events).first(count)) {
                if (const auto *failure = std::get_if<RendererFailure>(&event)) {
                    ADD_FAILURE() << failure->error.message;
                    return false;
                }
                if (const auto *failure = std::get_if<ViewportFailed>(&event)) {
                    ADD_FAILURE() << failure->error.message;
                    return false;
                }
                if (const auto *failure = std::get_if<ScenePrepareFailed>(&event)) {
                    ADD_FAILURE() << failure->error.message;
                    return false;
                }
                if (const auto *failure = std::get_if<ResourceStateChanged>(&event); failure && failure->state == RenderResourceState::FAILED) {
                    ADD_FAILURE() << failure->error.message;
                    return false;
                }
                if (const auto *finished = std::get_if<FrameCompleted>(&event); finished && finished->sequence == sequence)
                    complete = finished->submissionSerial != 0;
                if (auto *read = std::get_if<ViewportReadback>(&event); read && read->sequence == sequence && read->viewport == output)
                    pixels = std::move(read->bytes);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return complete && pixels.size() == 64 * 64 * 4;
    };
    auto first      = std::make_unique<FramePacket>();
    first->sequence = 1;
    first->commands.push_back(CreateScene{scene, {}});
    first->commands.push_back(CreateViewport{output, {ViewportKind::OFFSCREEN, {64, 64}}});
    ViewDesc camera;
    camera.scene  = scene;
    camera.output = output;
    first->commands.push_back(CreateView{{0, 1}, camera});
    RenderEntityDesc desc;
    desc.mesh        = resources->parts.front().mesh;
    desc.material    = resources->parts.front().material;
    desc.world.v[14] = .5f;
    first->commands.push_back(CreateRenderEntity{entity, desc});
    for (const auto &payload : resources->payloads)
        first->commands.push_back(ResourceCpuReady{payload});
    ASSERT_FALSE(RegisterScenePass(*first, {0, 1}, moduleLease, sources));
    std::vector<uint8_t> baseline, outlined, cleared, stale;
    ASSERT_TRUE(finish(std::move(first), baseline));
    EXPECT_GT(baseline[(32 * 64 + 32) * 4], 100u);
    auto second      = std::make_unique<FramePacket>();
    second->sequence = 2;
    second->extensions.push_back(SceneHighlightInput(Name("aurora.scene"), output, std::span{&entity, 1}));
    ASSERT_TRUE(finish(std::move(second), outlined));
    uint32_t outlinePixels = 0;
    for (size_t at = 0; at < baseline.size(); at += 4)
        if (!baseline[at] && !baseline[at + 1] && !baseline[at + 2] && outlined[at] > 100)
            ++outlinePixels;
    EXPECT_GT(outlinePixels, 32u);
    EXPECT_EQ(outlined[(32 * 64 + 32) * 4], baseline[(32 * 64 + 32) * 4]);
    auto third      = std::make_unique<FramePacket>();
    third->sequence = 3;
    ASSERT_TRUE(finish(std::move(third), cleared));
    EXPECT_EQ(cleared, baseline);
    auto fourth      = std::make_unique<FramePacket>();
    fourth->sequence = 4;
    constexpr RenderEntityRef staleEntity{scene, {0, 2}};
    fourth->extensions.push_back(SceneHighlightInput(Name("aurora.scene"), output, std::span{&staleEntity, 1}));
    ASSERT_TRUE(finish(std::move(fourth), stale));
    EXPECT_EQ(stale, baseline);
    host.Shutdown();
}

TEST(RendererModule, MaterialAssetInstanceNamesSampleExactImageVersionsThroughMetal)
{
    VerifyNamedMaterialAssets(false);
}

TEST(RendererModule, ShippedProductsMaterialAssetNamesAndImageVersionsWithoutCompiler)
{
    VerifyNamedMaterialAssets(true);
}

TEST(RendererModule, MetalCheckedDrawableRetiresBeforeResizeAndClose)
{
    struct NativeHost {
        ~NativeHost()
        {
            window.reset();
            Platform::Get()->Shutdown();
        }
        std::unique_ptr<NativeWindow> window;
    } host;
    ASSERT_TRUE(Platform::Get()->Init({}));
    host.window.reset(NativeWindow::Create({96, 64, "MetalCheckedOutput", "Metal checked output", nullptr}));
    ASSERT_TRUE(host.window);
    auto acquired = Instance::Get()->Acquire({"Metal checked output", "SkyEngine", true, API::METAL});
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<Instance::DeviceSession>>(acquired));
    auto  session = std::move(std::get<std::unique_ptr<Instance::DeviceSession>>(acquired));
    auto *device  = session->GetDevice();
    ASSERT_TRUE(device);
    SwapChainPtr output(device->CreateSwapChain({host.window->GetNativeHandle(), 96, 64}));
    ASSERT_TRUE(output);
    EXPECT_EQ(output->GetDevice(), device);
    EXPECT_EQ(output->QuerySurface().status, PresentationStatus::READY);
    EXPECT_EQ(output->TryAcquireFrame(1'000'001).status, PresentationStatus::FAILED);
    auto acquire = [&]() {
        SwapChainAcquireResult result;
        const auto             deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        do {
            const auto start = std::chrono::steady_clock::now();
            result           = output->TryAcquireFrame(0);
            EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(50));
            if (result.status != PresentationStatus::BUSY)
                return result;
            bool quit = false;
            Platform::Get()->PoolEvent(quit);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < deadline);
        return result;
    };
    std::vector<AcquiredSwapChainFrame> frames;
    for (uint32_t i = 0; i < output->GetImageCount(); ++i) {
        const auto frame = acquire();
        ASSERT_EQ(frame.status, PresentationStatus::READY);
        frames.push_back(frame.frame);
    }
    EXPECT_EQ(output->TryAcquireFrame(0).status, PresentationStatus::BUSY);
    EXPECT_EQ(output->TryResize(96, 64).status, PresentationStatus::BUSY);
    EXPECT_EQ(output->TryClose().status, PresentationStatus::BUSY);
    EXPECT_EQ(output->AcquireNextImage(nullptr, nullptr, 0), INVALID_INDEX);
    for (const auto frame : frames) {
        EXPECT_EQ(output->TryPresentFrame(frame.serial, false).status, PresentationStatus::BUSY);
        EXPECT_EQ(output->RetireFrame(frame.serial, false).status, FenceStatus::FAILED);
        EXPECT_EQ(output->RetireFrame(frame.serial, true).status, FenceStatus::SIGNALED);
        EXPECT_EQ(output->RetireFrame(frame.serial, true).status, FenceStatus::FAILED);
    }
    const auto frame = acquire();
    ASSERT_EQ(frame.status, PresentationStatus::READY);
    EXPECT_GT(frame.frame.serial, frames.back().serial);
    EXPECT_EQ(output->TryPresentFrame(frames.front().serial, true).status, PresentationStatus::FAILED);
    std::unique_ptr<DeviceFrameContext> context(device->CreateFrameContext({1, 1, session->GetEpoch()}));
    ASSERT_TRUE(context);
    auto begin = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begin));
    const auto      lease = std::get<DeviceFrameLease>(begin);
    auto            owner = std::make_shared<SwapChainPtr>(output);
    FrameGraphData  data;
    FrameGraphImage image;
    image.image             = FrameImageReference::Borrow(output->GetImage(frame.frame.imageIndex), lease, owner);
    image.descriptor.format = PixelFormat::BGRA8_UNORM;
    image.descriptor.extent = {96, 64, 1};
    image.descriptor.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_DST;
    image.initial           = ImageLayout::UNDEFINED;
    image.final             = ImageLayout::PRESENT;
    data.images.push_back(std::move(image));
    FrameRasterPass clear;
    clear.color = {1, 0};
    clear.clear = ClearValue{.1f, .5f, .8f, 1.f};
    data.passes.push_back(clear);
    data.imageExports.push_back({1, 0});
    auto graph    = std::make_shared<const FrozenFrameGraph>(1, std::move(data));
    auto compiled = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto                recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    FrameSubmissionSync sync;
    sync.owner = owner;
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded, sync)));
    graph.reset();
    plan.reset();
    bool       complete = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!complete && std::chrono::steady_clock::now() < deadline) {
        std::array<FrameCompletion, 1> completion;
        if (context->PollCompleted(completion)) {
            EXPECT_EQ(completion[0].status, FrameCompletionStatus::COMPLETE);
            complete = completion[0].status == FrameCompletionStatus::COMPLETE;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(complete);
    EXPECT_EQ(output->TryPresentFrame(frame.frame.serial, true).status, PresentationStatus::READY);
    EXPECT_EQ(output->TryPresentFrame(frame.frame.serial, true).status, PresentationStatus::FAILED);
    FenceQueryResult retired;
    const auto       retirementDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    do {
        retired = output->RetireFrame(frame.frame.serial, false);
        if (retired.status != FenceStatus::PENDING)
            break;
        bool quit = false;
        Platform::Get()->PoolEvent(quit);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < retirementDeadline);
    ASSERT_EQ(retired.status, FenceStatus::SIGNALED);
    EXPECT_EQ(output->TryResize(96, 64).status, PresentationStatus::READY);
    EXPECT_EQ(output->TryClose().status, PresentationStatus::READY);
    EXPECT_EQ(output->QuerySurface().status, PresentationStatus::SURFACE_LOST);
    EXPECT_EQ(output->TryAcquireFrame(0).status, PresentationStatus::SURFACE_LOST);
}
#endif

#if defined(_WIN32) || defined(SKY_PLATFORM_MACOS)
TEST(RendererModule, NativeRendererHostDrainsBeforeLoadedModuleAndWindowAreReleased)
{
    struct NativeHost {
        ~NativeHost()
        {
            window.reset();
            Platform::Get()->Shutdown();
        }
        std::unique_ptr<NativeWindow> window;
    } native;
    ASSERT_TRUE(Platform::Get()->Init({}));
    native.window.reset(NativeWindow::Create({128, 96, "AuroraNativeHost", "Aurora native host test", nullptr}));
    ASSERT_TRUE(native.window);
    const auto         handle = native.window->GetNativeHandle();
    struct ClearOutput final : IFrameGraphFeature {
        Name GetName() const override
        {
            return Name("native.clear");
        }
        FrameOperationResult BuildFrame(FrameGraphBuildContext &context, const FrameExtensionPayload &) override
        {
            const auto output = context.GetOutput({0, 1});
            if (!output)
                return FrameOperationComplete{};
            FrameRasterPass clear;
            clear.color = output->color;
            clear.clear = ClearValue{.1f, .5f, .8f, 1.f};
            return context.Raster(std::move(clear));
        }
    };
    {
        LoadedProvider loaded(true);
        auto          *provider = ProviderService::Get()->GetApi();
        ASSERT_TRUE(provider);
        auto lease = loaded.manager.RetainModule("Aurora.Render");
        ASSERT_TRUE(lease);
        RendererHost     host;
        RendererInitInfo info;
    #if defined(SKY_PLATFORM_MACOS)
        info.backend = API::METAL;
    #endif
        const auto initialized = host.Init(*provider, info);
        ASSERT_FALSE(initialized) << initialized->message;
        auto *renderer = host.Get();
        ASSERT_TRUE(renderer);
        native.window->SetCloseDeferred(true);
        uint64_t sequence = 1;
        auto     packet   = std::make_unique<FramePacket>();
        packet->sequence  = sequence;
        packet->featureCommands.push_back(RegisterRenderFeature{{0, 1}, Name("native.clear"), lease, std::make_shared<ClearOutput>()});
        ViewportDesc output;
        output.kind         = ViewportKind::NATIVE_WINDOW;
        output.extent       = {128, 96};
        output.format       = PixelFormat::BGRA8_UNORM;
        output.nativeWindow = reinterpret_cast<uintptr_t>(handle);
        packet->commands.push_back(CreateViewport{{0, 1}, output});
        packet->extensions.push_back({Name("native.clear"), {}, {{0, 1}}, {}});
        bool       complete = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!complete && std::chrono::steady_clock::now() < deadline) {
            bool quit = false;
            Platform::Get()->PoolEvent(quit);
            std::array<RendererEvent, 16> events;
            const auto                    count = renderer->PollEvents(events);
            for (const auto &event : std::span(events).first(count)) {
                if (const auto *frame = std::get_if<FrameCompleted>(&event))
                    complete |= frame->submissionSerial != 0;
                EXPECT_FALSE(std::holds_alternative<RendererFailure>(event));
                EXPECT_FALSE(std::holds_alternative<ViewportFailed>(event));
            }
            if (!complete) {
                const auto submitted = renderer->TrySubmit(packet);
                ASSERT_NE(submitted.status, PacketSubmitStatus::REJECTED) << submitted.error.message;
                if (submitted.status == PacketSubmitStatus::ACCEPTED) {
                    packet           = std::make_unique<FramePacket>();
                    packet->sequence = ++sequence;
                    packet->extensions.push_back({Name("native.clear"), {}, {{0, 1}}, {}});
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        ASSERT_TRUE(complete);
        EXPECT_TRUE(native.window->IsCloseDeferred());
        packet.reset();
        host.Shutdown();
        EXPECT_EQ(Interface<IRenderer>::Get()->GetApi(), nullptr);
        native.window->SetCloseDeferred(false);
    }
    EXPECT_EQ(Interface<IRendererProvider>::Get()->GetApi(), nullptr);
    #if defined(_WIN32)
    EXPECT_TRUE(IsWindow(static_cast<HWND>(handle)));
    #endif
    EXPECT_FALSE(native.window->IsCloseDeferred());
}

#endif

#if defined(AURORA_RENDERER_STATIC_FIXTURE)
TEST(RendererModule, DuplicateRegistrationDoesNotReplaceOrUnregisterOwner)
{
    auto owner     = std::unique_ptr<IModule>(sky_module_create_Aurora_Render_Contract());
    auto duplicate = std::unique_ptr<IModule>(sky_module_create_Aurora_Render_Contract());
    ASSERT_TRUE(owner->Init({}));
    auto *provider = ProviderService::Get()->GetApi();
    EXPECT_TRUE(owner->Init({}));
    EXPECT_FALSE(duplicate->Init({}));
    duplicate->Shutdown();
    EXPECT_EQ(ProviderService::Get()->GetApi(), provider);
    owner->Shutdown();
    EXPECT_EQ(ProviderService::Get()->GetApi(), nullptr);
    owner->Shutdown();
    EXPECT_EQ(ProviderService::Get()->GetApi(), nullptr);
}
#endif

TEST(RendererModule, B14LeaseRetainsModuleCodeBeyondManagerLifetime)
{
    std::shared_ptr<const void> lease;
    std::weak_ptr<const void>   weak;
    {
        LoadedProvider loaded(true);
        lease = loaded.manager.RetainModule("Aurora.Render");
        ASSERT_TRUE(lease);
        weak = lease;
        EXPECT_FALSE(loaded.manager.RetainModule("missing"));
        loaded.manager.UnLoadModules();
        EXPECT_FALSE(loaded.manager.RetainModule("Aurora.Render"));
        EXPECT_EQ(ProviderService::Get()->GetApi(), nullptr);
    }
    EXPECT_FALSE(weak.expired());
#if defined(_WIN32) && !defined(AURORA_RENDERER_STATIC_FIXTURE) && !defined(AURORA_RENDERER_PACKAGED_STATIC)
    EXPECT_NE(GetModuleHandleA("Aurora.Render.dll"), nullptr);
#endif
    lease.reset();
    EXPECT_TRUE(weak.expired());
#if defined(_WIN32) && !defined(AURORA_RENDERER_STATIC_FIXTURE) && !defined(AURORA_RENDERER_PACKAGED_STATIC)
    EXPECT_EQ(GetModuleHandleA("Aurora.Render.dll"), nullptr);
#endif
}

int main(int argc, char **argv)
{
    // Shared device and reflection callbacks must outlive the reloadable module.
    Instance::Get();
    AuroraReflection(SerializationContext::Get());
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
