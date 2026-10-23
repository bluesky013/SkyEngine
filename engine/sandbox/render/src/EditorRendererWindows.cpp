//
// Created on 2026/10/17.
//
#include <editor/render/EditorRenderer.h>

#include "EditorFramePacer.h"
#include "EditorViewportState.h"

#include <array>
#include <aurora/adaptor/renderer/SceneAssetSource.h>
#include <aurora/pipeline/SceneHighlight.h>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <core/logger/Logger.h>
#include <editor/core/resource/SandboxResources.h>
#include <editor/render/UiSnapshotProducer.h>
#include <framework/application/ModuleManager.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/platform/PlatformBase.h>
#include <framework/window/NativeWindowManager.h>

namespace sky::editor {
    using namespace aurora;
    namespace {
        constexpr const char                                            *UI_FEATURE = "editor.ui";
        constexpr ViewportId                                             CONTENT{0, 1};
        constexpr SceneId                                                SCENE{0, 1};
        std::variant<RendererError, std::shared_ptr<IFrameGraphFeature>> CompileUi(API api)
        {
            RenderTechniqueData code;
            code.backend = api;
            ShaderCompilerSlang compiler;
            for (const bool vertex : {true, false}) {
                ShaderCompileDesc desc;
                desc.source     = std::string(RenderUiShaderSource());
                desc.sourcePath = "aurora/adaptor/Ui.slang";
                desc.entry      = vertex ? "VSMain" : "FSMain";
                desc.stage      = vertex ? ShaderStageFlagBit::VS : ShaderStageFlagBit::FS;
                desc.target     = api == API::DX12 ? ShaderTarget::DXIL : api == API::METAL ? ShaderTarget::MSL : ShaderTarget::SPIRV;
                ShaderCompileResult compiled;
                if (!compiler.Compile(desc, compiled))
                    return RendererError{RendererErrorCode::INVALID_ARGUMENT, compiled.errorInfo};
                std::string error;
                if (!MergeShaderReflections(compiled.reflection, code.reflection, error))
                    return RendererError{RendererErrorCode::INVALID_ARGUMENT, std::move(error)};
                auto &artifact = vertex ? code.vertex : code.fragment;
                artifact       = std::make_shared<const ShaderStageArtifact>(std::move(compiled));
            }
            code.blend.blendEn  = true;
            code.blend.srcColor = BlendFactor::SRC_ALPHA;
            code.blend.dstColor = BlendFactor::ONE_MINUS_SRC_ALPHA;
            code.blend.srcAlpha = BlendFactor::ONE;
            code.blend.dstAlpha = BlendFactor::ONE_MINUS_SRC_ALPHA;
            return CreateUiRenderFeature(Name(UI_FEATURE), std::move(code));
        }
    } // namespace
    EditorRenderer::Surface::Surface(uint32_t id) : id(id), output(std::make_unique<EditorViewportState>(id + 1))
    {
    }
    EditorRenderer::Surface::~Surface() = default;
    bool EditorRenderer::Init(const std::string &, uint32_t, uint32_t, API api, bool preview, RendererVulkanMode mode)
    {
        if (host.Get())
            return false;
        auto *provider = Interface<IRendererProvider>::Get()->GetApi();
        auto *system   = Interface<ISystemNotify>::Get()->GetApi();
        auto *modules  = system ? system->GetModuleManager() : nullptr;
        moduleLease    = modules ? modules->RetainModule("SandboxModule") : nullptr;
        if (!provider || !moduleLease) {
            Fail({RendererErrorCode::NOT_READY, "Editor requires provider-only Aurora.Render and a loaded SandboxModule lease"});
            return false;
        }
        RendererInitInfo init;
        init.backend    = api;
        init.vulkanMode = mode;
#if SKY_ENABLE_GPU_VALIDATION
        init.enableValidation = true;
#endif
        if (auto error = host.Init(*provider, init)) {
            Fail(*error);
            Shutdown();
            return false;
        }
        auto compiled = CompileUi(host.Get()->GetInfo().backend);
        if (const auto *error = std::get_if<RendererError>(&compiled)) {
            Fail(*error);
            Shutdown();
            return false;
        }
        feature           = std::move(std::get<std::shared_ptr<IFrameGraphFeature>>(compiled));
        const auto bundle = Platform::Get()->GetBundleFileSystem();
        if (!bundle) {
            Fail({RendererErrorCode::NOT_READY, "Editor shader bundle is unavailable"});
            Shutdown();
            return false;
        }
        auto resources = LoadShaderResources(*bundle, host.Get()->GetInfo().backend, ShaderResourceMode::DEVELOPMENT);
        if (const auto *error = std::get_if<std::string>(&resources)) {
            Fail({RendererErrorCode::INVALID_ARGUMENT, *error});
            Shutdown();
            return false;
        }
        shaderFiles      = std::move(std::get<ShaderResources>(resources).sources);
        auto sceneSource = CreateSceneAssetSource(shaderFiles);
        if (!sceneSource) {
            Fail({.code = RendererErrorCode::INVALID_ARGUMENT, .message = "Scene asset source creation failed"});
            Shutdown();
            return false;
        }
        sceneBridge = std::make_unique<RenderSceneBridge>(host.Get()->GetInfo(), std::move(sceneSource));
        sceneBridge->EnableGeometrySnapshots(true);
        textures = std::make_unique<UiSnapshotProducer>(host.Get()->GetInfo().deviceEpoch);
#if defined(SKY_BUILD_FREETYPE)
        if (freeTypeFont.LoadFont(SandboxResources::Resolve("fonts/OpenSans-Regular.ttf")) && freeTypeFont.IsReady())
            textSystem = std::make_unique<ui::UITextSystem>(&freeTypeFont, textures.get(), 256);
        else
#endif
            textSystem = std::make_unique<ui::UITextSystem>(&builtinFont, textures.get(), 128);
        withPreview = preview;
        LOG_I("EditorRender", "Single renderer initialized; UI production stays on the host thread");
        return true;
    }
    bool EditorRenderer::SetSceneResources(std::shared_ptr<aurora::ISceneResourceSource> source)
    {
        if (started || sceneWorldBound || !host.Get() || !source)
            return false;
        viewportPicker->Reset();
        sceneBridge = std::make_unique<aurora::RenderSceneBridge>(host.Get()->GetInfo(), std::move(source));
        sceneBridge->EnableGeometrySnapshots(true);
        return true;
    }
    void EditorRenderer::Start()
    {
        if (!host.Get() || started)
            return;
        auto *window = NativeWindowManager::Get()->GetMainWindow();
        if (!window) {
            Fail({RendererErrorCode::NOT_READY, "Editor main window is unavailable"});
            return;
        }
        auto surface      = std::make_unique<Surface>(0);
        surface->window   = window;
        surface->extent   = {window->GetWidth(), window->GetHeight()};
        surface->dpiScale = window->GetDpiScale();
        window->SetCloseDeferred(true);
        Event<IWindowEvent>::Connect(window, this);
        surfaces.insert(surfaces.begin(), std::move(surface));
        pending           = std::make_unique<FramePacket>();
        pending->sequence = ++sequence;
        pending->featureCommands.push_back(RegisterRenderFeature{{0, 1}, Name(UI_FEATURE), moduleLease, feature});
        if (const auto error = RegisterScenePass(*pending, {1, 1}, moduleLease, shaderFiles)) {
            Fail(*error);
            return;
        }
        if (auto error = scene.Create(*pending, SCENE, {Name("editor")})) {
            Fail(*error);
            return;
        }
        pending->commands.push_back(CreateViewport{CONTENT, {ViewportKind::OFFSCREEN, sceneExtent}});
        submittedSceneExtent = sceneExtent;
        ViewDesc view;
        view.scene  = SCENE;
        view.output = CONTENT;
        pending->commands.push_back(CreateView{{0, 1}, view});
        started = true;
        if (withPreview)
            CreateSurface("SkyEngine Preview", 0, 0, 320, 180);
    }
    bool EditorRenderer::Submit()
    {
        if (!pending)
            return true;
        const auto result = framePacer->TrySubmit(*host.Get(), pending);
        if (result.status == PacketSubmitStatus::REJECTED)
            Fail(result.error);
        return result.status == PacketSubmitStatus::ACCEPTED;
    }
    void EditorRenderer::Poll()
    {
        std::array<RendererEvent, 64> events;
        size_t                        count = 0;
        std::vector<std::string>      closed;
        while ((count = host.Get()->PollEvents(events)) != 0) {
            for (const auto &event : std::span(events).first(count)) {
                framePacer->OnEvent(event);
                scene.OnEvent(event);
                if (sceneBridge)
                    sceneBridge->OnEvent(event);
                if (const auto *failure = std::get_if<RendererFailure>(&event))
                    Fail(failure->error);
                if (const auto *failure = std::get_if<ViewportFailed>(&event))
                    for (const auto &surface : surfaces)
                        if (surface->output->OnFailure(*failure))
                            LOG_W("EditorRender", "Viewport %u unavailable: %s", failure->viewport.index, failure->error.message.c_str());
                if (const auto *failure = std::get_if<ResourceStateChanged>(&event); failure && failure->state == RenderResourceState::FAILED)
                    LOG_W("EditorRender", "Resource unavailable: %s", failure->error.message.c_str());
                if (const auto *destroyed = std::get_if<ViewportDestroyed>(&event)) {
                    std::erase_if(surfaces, [&](const auto &surface) {
                        if (!surface->output->IsRetiring() || surface->output->Output() != destroyed->viewport)
                            return false;
                        if (const auto error = surface->output->OnDestroyed(*destroyed, surface->closed))
                            Fail(*error);
                        if (!surface->closed)
                            return false;
                        if (!surface->closeNotified && !surface->panelId.empty())
                            closed.push_back(surface->panelId);
                        Event<IWindowEvent>::DisConnect(surface->window, this);
                        surface->window->SetCloseDeferred(false);
                        return true;
                    });
                }
                if (rendererEventObserver)
                    rendererEventObserver(event);
            }
        }
        for (const auto &panel : closed)
            if (surfaceClosed)
                surfaceClosed(panel);
    }
    void EditorRenderer::Tick(float delta)
    {
        if (!started || !host.Get())
            return;
        Poll();
        if (host.Get()->GetState() != RendererState::RUNNING || !Submit())
            return;
        if (!framePacer->CanBuild())
            return;
        pending               = std::make_unique<FramePacket>();
        pending->sequence     = ++sequence;
        pending->deltaSeconds = delta;
        std::vector<std::string> closed;
        for (auto &surface : surfaces) {
            if (surface->closed && !surface->closeNotified) {
                surface->closeNotified = true;
                closed.push_back(surface->panelId);
            }
            const bool   created = surface->output->IsCreated();
            ViewportDesc desc{ViewportKind::NATIVE_WINDOW, surface->extent, PixelFormat::BGRA8_UNORM,
                              reinterpret_cast<uintptr_t>(surface->window->GetNativeHandle())};
            if (!surface->output->Prepare(*pending, desc, surface->closed, surface->window->RetainNativeHandle()))
                continue;
            if (!created) {
                surface->revision        = 0;
                surface->submittedExtent = surface->extent;
            } else if (surface->extent.width != surface->submittedExtent.width || surface->extent.height != surface->submittedExtent.height) {
                pending->commands.push_back(ResizeViewport{surface->output->Output(), surface->extent, ++surface->revision});
                surface->submittedExtent = surface->extent;
            }
            if (!surface->extent.width || !surface->extent.height)
                continue;
            surface->paint.Begin({0, 0, float(surface->extent.width), float(surface->extent.height)});
            if (withPreview && surface->panelId == "SkyEngine Preview")
                surface->paint.AddTexturedQuad({0, 0, float(surface->extent.width), float(surface->extent.height)}, {0, 0, 1, 1}, 1, 0xFFFFFFFF);
            else if (guiSource)
                guiSource(surface->id, surface->paint, surface->extent.width, surface->extent.height);
        }
        if (sceneExtent.width != submittedSceneExtent.width || sceneExtent.height != submittedSceneExtent.height) {
            pending->commands.push_back(ResizeViewport{CONTENT, sceneExtent, ++sceneRevision});
            submittedSceneExtent = sceneExtent;
        }
        if (sceneBridge && !sceneSource) {
            if (const auto error = sceneBridge->Append(*pending, SCENE, CONTENT)) {
                Fail(*error);
                return;
            }
            viewportPicker->Prepare(sceneBridge->GeometrySnapshot(), pickContext);
            if (!selectedActors.empty()) {
                const auto entities = sceneBridge->Entities(selectedActors, true);
                if (!entities.empty()) {
                    const auto color = [](uint32_t value) {
                        return Vector4{float(value & 255) / 255.f, float((value >> 8) & 255) / 255.f, float((value >> 16) & 255) / 255.f,
                                       float(value >> 24) / 255.f};
                    };
                    pending->extensions.push_back(SceneHighlightInput(
                        Name("aurora.scene"), CONTENT, entities,
                        {color(selectionStyle.visible), color(selectionStyle.occluded), std::clamp(selectionStyle.width, 1.f, 4.f)}));
                }
            }
            const auto &issues  = sceneBridge->GetIssues();
            const auto  message = issues.empty() ? std::string{} : issues.front().message;
            if (!message.empty() && message != sceneIssue)
                LOG_W("EditorRender", "Scene input: %s", message.c_str());
            sceneIssue = message;
            ViewDesc view;
            view.scene          = SCENE;
            view.output         = CONTENT;
            view.cameraPosition = {0, 0, 5};
            view.view.v[14]     = -5;
            std::fill(std::begin(view.projection.v), std::end(view.projection.v), 0.f);
            view.projection.v[0]  = 2.4142136f * float(sceneExtent.height) / float(sceneExtent.width);
            view.projection.v[5]  = 2.4142136f;
            view.projection.v[10] = -1.0001f;
            view.projection.v[11] = -1;
            view.projection.v[14] = -.10001f;
            if (editorCamera) {
                view.cameraPosition   = editorCamera->position;
                const auto matrix     = editorCamera->View();
                const auto projection = editorCamera->Projection(float(sceneExtent.width) / float(sceneExtent.height));
                std::copy(std::begin(matrix.v), std::end(matrix.v), view.view.v);
                std::copy(std::begin(projection.v), std::end(projection.v), view.projection.v);
            } else if (auto camera = sceneBridge->Camera(SCENE, CONTENT, sceneExtent))
                view = *camera;
            pending->commands.push_back(UpdateView{{0, 1}, view, ++viewRevision});
        }
        if (sceneSource)
            sceneSource(*pending, scene.GetSceneId());
        // All windows finish glyph generation before snapshots select the final atlas revision.
        for (const auto &surface : surfaces) {
            if (surface->closed || surface->output->IsRetiring() || !surface->extent.width || !surface->extent.height)
                continue;
            auto snapshot = textures->Snapshot(Name(UI_FEATURE), surface->output->Output(), surface->extent, surface->paint.GetDrawData(),
                                               {CONTENT, sceneRevision});
            if (const auto *error = std::get_if<RendererError>(&snapshot)) {
                Fail(*error);
                return;
            }
            pending->extensions.push_back(std::move(std::get<FrameExtensionPayload>(snapshot)));
        }
        textures->Flush(*pending);
        Submit();
        std::erase_if(surfaces, [&](const auto &surface) {
            if (!surface->closed || surface->output->IsCreated())
                return false;
            Event<IWindowEvent>::DisConnect(surface->window, this);
            surface->window->SetCloseDeferred(false);
            return true;
        });
        for (const auto &panel : closed)
            if (surfaceClosed && !panel.empty())
                surfaceClosed(panel);
    }
    void EditorRenderer::Fail(const RendererError &error)
    {
        LOG_E("EditorRender", "%s", error.message.c_str());
        if (host.Get())
            host.Get()->RequestShutdown();
        if (auto *system = Interface<ISystemNotify>::Get()->GetApi())
            system->SetExit();
    }
    void EditorRenderer::SetWorld(World *world)
    {
        if (sceneBridge) {
            sceneWorldBound = world != nullptr;
            sceneBridge->SetWorld(world);
        }
    }
    void EditorRenderer::SetSceneExtent(Extent2D extent)
    {
        if (extent.width && extent.height && extent.width <= 4096 && extent.height <= 4096)
            sceneExtent = extent;
    }
    void EditorRenderer::Shutdown()
    {
        started               = false;
        rendererEventObserver = {};
        for (const auto &surface : surfaces)
            Event<IWindowEvent>::DisConnect(surface->window, this);
        guiSource   = {};
        sceneSource = {};
        pending.reset();
        host.Shutdown();
        framePacer->Reset();
        viewportPicker->Reset();
        sceneBridge.reset();
        sceneWorldBound = false;
        shaderFiles.reset();
        scene.OnRendererStopped();
        feature.reset();
        moduleLease.reset();
        textSystem.reset();
        textures.reset();
        for (auto &surface : surfaces)
            surface->window->SetCloseDeferred(false);
        surfaces.clear();
        sequence      = 0;
        nextSurfaceId = 1;
        sceneRevision = viewRevision = 0;
        sceneIssue.clear();
    }
    uint32_t EditorRenderer::CreateSurface(const std::string &panel, int x, int y, uint32_t width, uint32_t height)
    {
        if (!host.Get() || !width || !height || surfaces.size() >= 63 || nextSurfaceId == UINT32_MAX - 1)
            return 0;
        auto surface     = std::make_unique<Surface>(nextSurfaceId++);
        surface->panelId = panel;
        const auto name  = "SkyEnginePanel" + std::to_string(surface->id);
        surface->owned.reset(NativeWindow::Create({width, height, name.c_str(), panel.c_str(), nullptr}));
        if (!surface->owned)
            return 0;
        surface->window = surface->owned.get();
        surface->window->SetPosition(x, y);
        surface->window->SetCloseDeferred(true);
        surface->extent   = {surface->window->GetWidth(), surface->window->GetHeight()};
        surface->dpiScale = surface->window->GetDpiScale();
        Event<IWindowEvent>::Connect(surface->window, this);
        const auto id = surface->id;
        surfaces.push_back(std::move(surface));
        return id;
    }
    void EditorRenderer::DestroySurface(uint32_t id)
    {
        for (auto &surface : surfaces)
            if (surface->id == id)
                surface->closed = true;
    }
    uint32_t EditorRenderer::SurfaceIdForWindow(const NativeWindow *window) const
    {
        for (const auto &surface : surfaces)
            if (surface->window == window)
                return surface->id;
        return 0;
    }
    float EditorRenderer::SurfaceDpiScale(uint32_t id) const
    {
        for (const auto &surface : surfaces)
            if (surface->id == id)
                return surface->dpiScale;
        return 1;
    }
    void EditorRenderer::OnWindowClose(const NativeWindow *window)
    {
        for (auto &surface : surfaces)
            if (surface->window == window)
                surface->closed = true;
    }
    void EditorRenderer::OnWindowMove(const WindowMoveEvent &event)
    {
        for (const auto &surface : surfaces)
            if (surface->window->GetWinId() == event.winID && surface->id && surfaceGeometryChanged)
                surfaceGeometryChanged(surface->panelId, float(event.x), float(event.y), float(surface->extent.width), float(surface->extent.height));
    }
    void EditorRenderer::OnWindowResize(const WindowResizeEvent &event)
    {
        for (auto &surface : surfaces)
            if (surface->window->GetWinId() == event.winID) {
                surface->extent = {event.width, event.height};
                if (surface->id && surfaceGeometryChanged) {
                    int32_t x = 0, y = 0;
                    surface->window->GetPosition(x, y);
                    surfaceGeometryChanged(surface->panelId, float(x), float(y), float(event.width), float(event.height));
                }
            }
    }
} // namespace sky::editor
