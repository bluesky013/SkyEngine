//
// Created on 2026/10/17.
//
#include "HostSceneFixture.h"
#include <aurora/shader/ShaderCompilerSlang.h>
#include <aurora/adaptor/AuroraModule.h>
#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/components/StaticMeshComponent.h>
#include <aurora/adaptor/renderer/SceneAssetSource.h>
#include <aurora/pipeline/SceneHighlight.h>
#include <aurora/rhi/Instance.h>
#include <aurora/shader/ShaderResolver.h>
#include <chrono>
#include <editor/render/EditorRenderer.h>
#include <editor/shell/EditorShell.h>
#include <editor/shell/ReflectedInspectorPanel.h>
#include <editor/shell/SceneViewport.h>
#include <editor/shell/panels/ServicePanels.h>
#include <framework/application/ModuleManager.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/platform/PlatformBase.h>
#include <framework/window/NativeWindow.h>
#include <framework/world/TransformComponent.h>
#include <thread>

#include <D3D12Device.h>
#include <d3d12sdklayers.h>
#include <windows.h>

using namespace sky;
using namespace sky::aurora;
using namespace std::chrono_literals;
namespace {
    class EditorHostNativeTest : public testing::TestWithParam<uint32_t>, public ISystemNotify {
    protected:
        void SetUp() override
        {
            previousCompiler = ShaderCompilerFactory::Get().GetCompiler();
            ShaderCompilerFactory::Get().Register(&compiler);
            ASSERT_TRUE(Platform::Get()->Init({}));
            window.reset(NativeWindow::Create({160, 120, "B14EditorHost", "Editor host test", nullptr}));
            ASSERT_TRUE(window);
            Interface<ISystemNotify>::Get()->Register(*this);
            modules.AddStaticModule("SandboxModule", std::make_unique<IModule>());
            modules.TopoSort();
            modules.LoadModules({});
            StartArguments args;
            args.args = {"EditorHostTest", "--aurora-provider-only"};
            ASSERT_TRUE(provider.Init(args));
            ASSERT_EQ(Interface<IRenderer>::Get()->GetApi(), nullptr);
            ASSERT_TRUE(editor.Init("B14", 160, 120, GetParam() == 2 ? API::DX12 : API::VULKAN, false,
                                    GetParam() == 1 ? RendererVulkanMode::RENDER_PASS : RendererVulkanMode::DYNAMIC_RENDERING));
            renderer = Interface<IRenderer>::Get()->GetApi();
            ASSERT_TRUE(renderer);
#if SKY_ENABLE_GPU_VALIDATION
            if (GetParam() == 2) {
                ASSERT_TRUE(
                    SUCCEEDED(static_cast<D3D12Device *>(Instance::Get()->GetDevice())->GetNativeHandle()->QueryInterface(IID_PPV_ARGS(&debug))));
                debug->ClearStoredMessages();
            }
#endif
        }
        void TearDown() override
        {
            editor.Shutdown();
            provider.Shutdown();
            modules.UnLoadModules();
            EXPECT_EQ(Interface<IRenderer>::Get()->GetApi(), nullptr);
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
            Interface<ISystemNotify>::Get()->UnRegister();
            window.reset();
            Platform::Get()->Shutdown();
            Instance::Destroy();
            ShaderCompilerFactory::Get().Register(previousCompiler);
        }
        ModuleManager *GetModuleManager() const override
        {
            return const_cast<ModuleManager *>(&modules);
        }
        void *GetMainWindowHandle() const override
        {
            return window->GetNativeHandle();
        }
        void SetExit() override
        {
            failed = true;
        }
        template <typename Predicate>
        bool Until(Predicate done)
        {
            const auto end = std::chrono::steady_clock::now() + 10s;
            do {
                bool quit = false;
                Platform::Get()->PoolEvent(quit);
                if (done())
                    return true;
                editor.Tick(1.f / 60);
                std::this_thread::sleep_for(2ms);
            } while (std::chrono::steady_clock::now() < end);
            return false;
        }
        ShaderCompilerSlang                     compiler;
        IShaderCompiler                        *previousCompiler = nullptr;
        ModuleManager                           modules;
        AuroraModule                            provider;
        editor::EditorRenderer                  editor;
        std::unique_ptr<NativeWindow>           window;
        IRenderer                              *renderer = nullptr;
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> debug;
        bool                                    failed = false;
    };
} // namespace
TEST_P(EditorHostNativeTest, B14SceneUiAndToolWindowRetireOnOneRenderer)
{
    uint32_t painted = 0;
    editor.SetGuiSource([&](uint32_t, ui::UIPaintContext &paint, uint32_t w, uint32_t h) {
        ++painted;
        paint.AddRect({0, 0, float(w), float(h)}, 0xFF101010);
        paint.AddRoundedRect({8, 8, 64, 48}, 0xFF00FF00, 8);
        paint.AddTexturedQuad({64, 8, float(w), float(h)}, {0, 0, 1, 1}, 1, 0xFFFFFFFF);
    });
    bool populated = false;
    editor.SetSceneSource([&](FramePacket &packet, SceneId scene) {
        if (!populated) {
            AddHostMesh(packet, scene, renderer->GetInfo());
            populated = true;
        }
        packet.readbacks.push_back({0, 1});
    });
    editor.Start();
    const auto tool = editor.CreateSurface("B14 Tool", 0, 0, 128, 96);
    ASSERT_NE(tool, 0u);
    auto toolWindow = FindWindowA(nullptr, "B14 Tool");
    ASSERT_TRUE(toolWindow);
    bool readbackSeen = false;
    editor.SetReadbackCallback([&](const ViewportReadback &pixels) {
        const size_t center = (90 * 320 + 160) * 4;
        if (pixels.bytes[center] > 240 && pixels.bytes[center + 1] < 10)
            readbackSeen = true;
    });
    ASSERT_TRUE(Until([&] { return readbackSeen; }));
    EXPECT_GT(painted, 1u);
    EXPECT_EQ(Interface<IRenderer>::Get()->GetApi(), renderer);
    SetWindowPos(toolWindow, nullptr, 0, 0, 192, 144, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    SendMessageW(toolWindow, WM_CLOSE, 0, 0);
    EXPECT_TRUE(IsWindow(toolWindow));
    ASSERT_TRUE(Until([&] { return !IsWindow(toolWindow); }));
    const auto main = static_cast<HWND>(window->GetNativeHandle());
    SetWindowPos(main, nullptr, 0, 0, 240, 180, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    editor.Tick(1.f / 60);
    SendMessageW(main, WM_CLOSE, 0, 0);
    EXPECT_TRUE(IsWindow(main));
    editor.Shutdown();
    EXPECT_TRUE(IsWindow(main));
    EXPECT_FALSE(window->IsCloseDeferred());
    EXPECT_FALSE(failed);
}
INSTANTIATE_TEST_SUITE_P(Backends, EditorHostNativeTest, testing::Values(0u, 1u, 2u));

TEST_P(EditorHostNativeTest, SelectionOutlineUsesSceneDepthAndClearsWithoutChangingMaterials)
{
    editor.SetSceneExtent({160, 120});
    uint32_t                     mode      = 0;
    bool                         populated = false;
    std::map<uint64_t, uint32_t> modes;
    std::vector<uint8_t>         baseline, outlined, cleared, stale;
    editor.SetSceneSource([&](FramePacket &packet, SceneId scene) {
        if (!populated) {
            auto           mesh = HostMesh(renderer->GetInfo());
            const float    positions[]{-.5f, -.5f, 0, -.5f, .5f, 0, .5f, -.5f, 0, .5f, .5f, 0};
            const uint16_t indices[]{0, 1, 2, 2, 1, 3};
            mesh.bytes.resize(60);
            std::memcpy(mesh.bytes.data(), positions, sizeof(positions));
            std::memcpy(mesh.bytes.data() + 48, indices, sizeof(indices));
            mesh.metadata.bounds = AABB{{-.5f, -.5f, 0}, {.5f, .5f, 0}};
            mesh.mesh            = RenderMeshData{{{0, 48, 12}}, {{0, 0, 0, Format::F_RGB32, "POSITION", 0}}, {{48, 12, IndexType::U16, {6}}}};
            auto material        = HostMaterial(renderer->GetInfo());
            material.material->techniques.front().program = CompileHostRaster(
                renderer->GetInfo(),
                "#include <aurora/pipeline/OpaquePass.slangh>\n#include <aurora/pipeline/SceneBatch.slangh>\n"
                "float4 VSMain(float3 p:POSITION):SV_Position { return mul(gOpaquePass.viewProjection,mul(gSceneBatch.world,float4(p,1))); }\n"
                "float4 FSMain():SV_Target { return float4(.1,.1,.1,1); }");
            material.material->techniques.front().program.depth = {.test = true, .write = true};
            material.material->techniques.front().parameters    = {};
            RenderEntityDesc selected;
            selected.mesh        = {mesh.resource, 1};
            selected.material    = {material.resource, 1};
            selected.world.v[14] = .6f;
            packet.commands.push_back(CreateRenderEntity{{scene, {0, 1}}, selected});
            auto blocker        = selected;
            blocker.world.v[12] = -.5f;
            blocker.world.v[14] = .3f;
            packet.commands.push_back(CreateRenderEntity{{scene, {1, 1}}, blocker});
            packet.commands.push_back(ResourceCpuReady{std::move(mesh)});
            packet.commands.push_back(ResourceCpuReady{std::move(material)});
            populated = true;
        }
        if (mode == 1 || mode == 3) {
            const RenderEntityRef selected{scene, {0, mode == 3 ? 2u : 1u}};
            packet.extensions.push_back(SceneHighlightInput(Name("aurora.scene"), {0, 1}, std::span{&selected, 1}));
        }
        modes[packet.sequence] = mode;
        packet.readbacks.push_back({0, 1});
    });
    editor.SetRendererEventObserver([&](const RendererEvent &event) {
        if (const auto *error = std::get_if<RendererFailure>(&event))
            ADD_FAILURE() << error->error.message;
        const auto *pixels = std::get_if<ViewportReadback>(&event);
        if (!pixels || !modes.contains(pixels->sequence) || pixels->bytes.size() != 160 * 120 * 4)
            return;
        const auto phase = modes[pixels->sequence];
        if (phase == 0 && pixels->bytes[(60 * 160 + 100) * 4] > 20)
            baseline = pixels->bytes;
        if (phase == 1)
            outlined = pixels->bytes;
        if (phase == 2)
            cleared = pixels->bytes;
        if (phase == 3)
            stale = pixels->bytes;
    });
    editor.Start();
    ASSERT_TRUE(Until([&] { return !baseline.empty(); }));
    mode = 1;
    ASSERT_TRUE(Until([&] { return !outlined.empty(); }));
    uint32_t visible = 0, hidden = 0, interiorChanges = 0;
    for (uint32_t y = 0; y < 120; ++y)
        for (uint32_t x = 0; x < 160; ++x) {
            const size_t at    = (y * 160 + x) * 4;
            const int    delta = int(outlined[at]) - int(baseline[at]);
            if (x > 80 && delta > 150)
                ++visible;
            if (x < 70 && delta > 30 && delta < 100)
                ++hidden;
            if (x > 85 && x < 110 && y > 40 && y < 80 && delta != 0)
                ++interiorChanges;
        }
    EXPECT_GT(visible, 100u);
    EXPECT_GT(hidden, 80u);
    EXPECT_EQ(interiorChanges, 0u);
    RecordProperty("visible_outline_pixels", visible);
    RecordProperty("occluded_outline_pixels", hidden);
    mode = 2;
    ASSERT_TRUE(Until([&] { return !cleared.empty(); }));
    EXPECT_EQ(cleared, baseline);
    mode = 3;
    ASSERT_TRUE(Until([&] { return !stale.empty(); }));
    EXPECT_EQ(stale, baseline);
    EXPECT_FALSE(failed);
    editor.Shutdown();
}

TEST_P(EditorHostNativeTest, StaticPickingUsesProductionSceneAndSharedSelection)
{
    using namespace sky::editor;
    SetWindowPos(static_cast<HWND>(window->GetNativeHandle()), nullptr, 0, 0, 1000, 800, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    auto *reflection = SerializationContext::Get();
    if (!TypeInfo<TransformComponent>::RegisteredId())
        TransformComponent::Reflect(reflection);
    AuroraReflection(reflection);
    WorldDocument          document("");
    SelectionService       selection;
    CommandService         commands;
    PropertyChangeNotifier changes;
    auto                   controller = std::make_shared<ViewportController>(selection, commands, changes);
    controller->Bind(&document);
    controller->SetPicker(editor.GetViewportPicker());
    controller->SetTool(GizmoTool::SELECT);
    struct Properties : RegisteredPropertySource {
        std::vector<EditorOutlineItem> rows;
        std::vector<EditorOutlineItem> Outline() const override
        {
            return rows;
        }
    } properties;
    const auto make = [&](const char *name, Vector3 location) {
        auto *actor = document.GetWorld()->CreateActor(name);
        auto *mesh  = actor->AddComponent<StaticMeshComponent>();
        mesh->SetMeshUuid(ScenePrimitiveId(ScenePrimitive::CUBE));
        mesh->SetMaterialUuid(SceneLitMaterialId());
        auto *transform = actor->GetComponent<TransformComponent>();
        transform->SetLocalTranslation(location);
        properties.Add(actor->GetUuid(), {transform, GetTypeNode(TypeInfo<TransformComponent>::RegisteredId())});
        properties.rows.push_back({actor->GetUuid(), name});
        return actor;
    };
    struct AuthoredMaterial : Asset<Material> {
        AuthoredMaterial()
        {
            status = Status::LOADED;
            SetType(Name(AssetTraits<Material>::ASSET_TYPE.data()));
        }
    };
    const auto authoredId = Uuid::CreateWithSeed(9871);
    auto       authored   = std::make_shared<AuthoredMaterial>();
    authored->SetUuid(authoredId);
    MaterialTechniqueData technique;
    technique.tag                                = "opaque";
    technique.program                            = {.vertex   = {.relativePath = "aurora/materials/Unlit.slang", .entry = "VSMain"},
                                                    .fragment = {.relativePath = "aurora/materials/Unlit.slang", .entry = "FSMain"}};
    technique.renderState.depthStencil.depthTest = technique.renderState.depthStencil.depthWrite = true;
    technique.renderState.depthStencil.compareOp                                                 = CompareOp::LESS_OR_EQUAL;
    technique.aliases           = {{"tint", "gMaterial.tint"}, {"surface", "gMaterial.surface"}};
    authored->Data().techniques = {technique};
    authored->Data().properties = {{.name = "tint", .type = MaterialPropertyType::FLOAT4, .value = Vector4{.2f, .6f, .9f, 1}},
                                   {.name = "surface", .type = MaterialPropertyType::FLOAT4, .value = Vector4{.5f, 0, 0, 0}}};
    auto       capabilities     = std::make_shared<SceneMaterialCapabilitySource>();
    const auto supportedSurface = std::make_shared<bool>(true);
    capabilities->resolve       = [authoredId, supportedSurface](const Uuid &id, const MaterialAssetData &, uint64_t revision) {
        return id == authoredId && *supportedSurface ? SceneSurfaceFacts::StaticOpaque(revision) : SceneSurfaceFacts{};
    };
    auto        files      = std::make_shared<ShaderFileSystem>();
    auto        shaderRoot = Platform::Get()->GetBundleFileSystem()->CreateSubSystem("assets/shaders", false);
    std::string shaderError;
    ASSERT_TRUE(files->LoadBundles(*shaderRoot, "", &shaderError)) << shaderError;
    auto resources = CreateSceneAssetSource(
        files, [authored](const Uuid &id) -> AssetPtr { return id == authored->GetUuid() ? authored : AssetPtr{}; }, {}, capabilities);
    const auto initialResource = resources->ResolveMesh(ScenePrimitiveId(ScenePrimitive::CUBE), authoredId, renderer->GetInfo());
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(initialResource));
    const auto initialOwned = std::get<std::shared_ptr<const SceneMeshResources>>(initialResource);
    ASSERT_TRUE(initialOwned->parts[0].surface.SupportsGeometry());
    resources->Invalidate(authoredId);
    const auto revisedResource = resources->ResolveMesh(ScenePrimitiveId(ScenePrimitive::CUBE), authoredId, renderer->GetInfo());
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(revisedResource));
    const auto revisedOwned = std::get<std::shared_ptr<const SceneMeshResources>>(revisedResource);
    EXPECT_EQ(initialOwned->parts[0].mesh.resource, revisedOwned->parts[0].mesh.resource);
    EXPECT_NE(initialOwned->parts[0].surface.revision, revisedOwned->parts[0].surface.revision);
    ASSERT_TRUE(editor.SetSceneResources(resources));
    auto *back  = make("Back without collider", {0, 0, 0});
    auto *front = make("Front without collider", {0, 0, 2});
    back->GetComponent<StaticMeshComponent>()->SetMaterialUuid(authoredId);
    ASSERT_EQ(back->GetComponents().size(), 2u);
    ReflectedInspectorPanel inspector(nullptr, &properties, &selection, "Inspector");
    LayoutModel             layout;
    layout.SetDefault({"viewport"});
    PanelRegistry panels;
    panels.Register({"viewport", "Viewport"});
    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&panels);
    shell.SetSelection(&selection);
    shell.RegisterPanelView("viewport", [&]() {
        auto viewport = std::make_unique<SceneViewport>(controller);
        viewport->SetTexture(1);
        viewport->extentChanged = [&](Vector2 size) {
            const auto width = uint32_t(std::max(size.x, 1.f)), height = uint32_t(std::max(size.y, 1.f));
            controller->SetPickImageExtent(width, height);
            editor.SetSceneExtent({width, height});
        };
        return viewport;
    });
    shell.Rebuild();
    editor.SetWorld(document.GetWorld());
    editor.SetEditorCamera(controller->camera);
    editor.SetPickContext(controller->PickContext());
    editor.SetGuiSource([&](uint32_t surface, ui::UIPaintContext &paint, uint32_t width, uint32_t height) {
        shell.PaintSurface(surface, paint, float(width), float(height));
    });
    editor.Start();
    const auto picker = editor.GetViewportPicker();
    ASSERT_TRUE(Until([&] { return picker->PreparationSerial() > 3 && shell.GetPanelView("viewport")->GetBounds().Width() > 600; }));
    auto                clickStart = std::chrono::steady_clock::now();
    std::vector<double> clickTimes;
    const auto          click = [&](uint32_t surface, float horizontal, float vertical = .5f) {
        clickStart = std::chrono::steady_clock::now();
        controller->SetPickSurface(surface);
        const auto         rect = shell.GetPanelView("viewport")->GetBounds();
        ui::UIPointerEvent event;
        event.x      = rect.left + rect.Width() * horizontal;
        event.y      = rect.top + rect.Height() * vertical;
        event.action = ui::UIPointerAction::DOWN;
        shell.DispatchPointerToSurface(surface, event);
        event.action = ui::UIPointerAction::UP;
        shell.DispatchPointerToSurface(surface, event);
    };
    const auto resolve = [&] {
        editor.SetPickContext(controller->PickContext());
        const bool done = Until([&] {
            controller->ProcessPicking();
            return !controller->HasGesture();
        });
        clickTimes.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - clickStart).count());
        return done;
    };
    click(0, .5f);
    ASSERT_TRUE(controller->HasGesture()) << controller->Error();
    ASSERT_TRUE(resolve());
    ASSERT_EQ(selection.GetSelection().size(), 1u) << controller->Error();
    EXPECT_EQ(selection.GetSelection()[0].id, front->GetUuid());
    EXPECT_TRUE(inspector.Form().IsValid());
    OutlinerPanel outliner(&selection, nullptr, "Outliner", &properties);
    outliner.Arrange({0, 0, 300, 300});
    ui::UIPaintContext outlinePaint;
    outlinePaint.Begin({0, 0, 300, 300});
    outliner.OnPaint(outlinePaint);
    EXPECT_TRUE(std::any_of(outlinePaint.GetDrawData().vertices.begin(), outlinePaint.GetDrawData().vertices.end(),
                            [](const auto &vertex) { return vertex.color == GetDefaultUiTheme().colors.rowSelected; }));
    controller->SetTool(GizmoTool::TRANSLATE);
    GizmoGeometry geometry;
    EXPECT_TRUE(controller->BuildGeometry({160, 120}, {}, geometry));
    EXPECT_FALSE(geometry.empty());
    controller->SetTool(GizmoTool::SELECT);
    front->GetComponent<TransformComponent>()->SetLocalTranslation({4, 0, 2});
    const auto oldRevision = picker->Context().scene;
    ASSERT_TRUE(Until([&] { return picker->Context().scene != oldRevision; }));
    click(0, .5f);
    ASSERT_TRUE(resolve());
    ASSERT_EQ(selection.GetSelection().size(), 1u);
    EXPECT_EQ(selection.GetSelection()[0].id, back->GetUuid());
    const auto tool = editor.CreateSurface("Picking tool", 0, 0, 320, 240);
    ASSERT_NE(tool, 0u);
    FloatingPanel floating;
    floating.panelId = "viewport";
    shell.FloatPanelToSurface("viewport", tool, floating);
    ASSERT_TRUE(Until([&] { return shell.GetPanelView("viewport")->GetBounds().Width() == 320; }));
    selection.Clear();
    click(tool, .5f);
    ASSERT_TRUE(resolve());
    ASSERT_EQ(selection.GetSelection().size(), 1u);
    EXPECT_EQ(selection.GetSelection()[0].id, back->GetUuid());
    const auto rotatedRevision = picker->Context().scene;
    back->GetComponent<TransformComponent>()->SetLocalRotation(Quaternion(.785398163f, Vector3{0, 0, 1}));
    ASSERT_TRUE(Until([&] { return picker->Context().scene != rotatedRevision; }));
    const auto bounds = shell.GetPanelView("viewport")->GetBounds();
    Vector2    corner;
    ASSERT_TRUE(controller->camera.Project({.65f, .65f, .5f}, {bounds.Width(), bounds.Height()}, corner));
    click(tool, corner.x / bounds.Width(), corner.y / bounds.Height());
    ASSERT_TRUE(resolve());
    EXPECT_TRUE(selection.IsEmpty());
    click(tool, .05f);
    ASSERT_TRUE(resolve());
    EXPECT_TRUE(selection.IsEmpty());
    EXPECT_FALSE(inspector.Form().IsValid());
    struct IconProvider : IViewportPickProvider {
        std::shared_ptr<ViewportPickProxySnapshot> snapshot = std::make_shared<ViewportPickProxySnapshot>();
        uint64_t                                   Id() const override
        {
            return 9921;
        }
        bool RequiresModuleLease() const override
        {
            return true;
        }
        std::shared_ptr<const ViewportPickProxySnapshot> Snapshot() override
        {
            return snapshot;
        }
    };
    auto *iconActor                  = document.GetWorld()->CreateActor("Pick provider icon");
    auto  iconProvider               = std::make_shared<IconProvider>();
    iconProvider->snapshot->revision = 1;
    iconProvider->snapshot->proxies.push_back({.target = {.actor = iconActor->GetUuid(), .generation = 1},
                                               .shape  = ViewportBillboardProxy{{0, 0, 0}, {32, 32}},
                                               .depth  = ViewportProxyDepth::OVERLAY});
    auto registration = picker->RegisterProvider(iconProvider, modules.RetainModule("SandboxModule"));
    ASSERT_TRUE(registration) << registration.message;
    const auto prepared = picker->PreparationSerial();
    ASSERT_TRUE(Until([&] { return picker->PreparationSerial() > prepared + 2; }));
    EXPECT_FALSE(picker->Overlay(controller->camera, {320, 240}).empty());
    click(tool, .5f);
    ASSERT_TRUE(resolve()) << controller->Error();
    ASSERT_EQ(selection.GetSelection().size(), 1u);
    EXPECT_EQ(selection.GetSelection()[0].id, iconActor->GetUuid());
    registration.scope.reset();
    const auto retired = picker->PreparationSerial();
    ASSERT_TRUE(Until([&] { return picker->PreparationSerial() > retired + 2; }));
    EXPECT_TRUE(picker->Overlay(controller->camera, {320, 240}).empty());
    *supportedSurface            = false;
    const auto supportedRevision = picker->Context().scene;
    resources->Invalidate(authoredId);
    ASSERT_TRUE(Until([&] { return picker->Context().scene != supportedRevision; }));
    click(tool, .5f);
    ASSERT_TRUE(resolve());
    ASSERT_EQ(selection.GetSelection().size(), 1u);
    EXPECT_EQ(selection.GetSelection()[0].id, iconActor->GetUuid());
    EXPECT_FALSE(controller->Error().empty());
    std::sort(clickTimes.begin(), clickTimes.end());
    RecordProperty("click_samples", int(clickTimes.size()));
    RecordProperty("click_p50_ms", std::to_string(clickTimes[clickTimes.size() / 2]));
    RecordProperty("click_p95_ms", std::to_string(clickTimes[size_t(std::ceil(double(clickTimes.size()) * .95)) - 1]));
    RecordProperty("click_max_ms", std::to_string(clickTimes.back()));
    EXPECT_FALSE(document.IsDirty());
    EXPECT_EQ(commands.GetUndoCount(), 0u);
    EXPECT_FALSE(failed);
    editor.SetGuiSource({});
    editor.SetWorld(nullptr);
    controller->Bind(nullptr);
    editor.Shutdown();
}
