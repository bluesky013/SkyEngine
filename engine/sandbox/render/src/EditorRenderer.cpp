#if !defined(_WIN32)
//
// Created on 2026/09/21.
//
// Editor-owned renderer. Owns the Aurora device, the main-window ClientViewport
// and the hardcoded frame (scene pass + UI pass). The GUI pipeline lives in
// sky::ui::UIRenderer (UIRender). Frame flow:
//   BeginFrame -> Acquire -> barrier -> scene pass (clear) -> UI pass (LOAD,
//   UIRenderer draws UIDrawData) -> barrier -> Submit -> present -> EndFrame.
//

    #include <editor/render/EditorRenderer.h>

#include "EditorFrameSubmission.h"
#include "WindowEventBinding.h"

#include <editor/core/resource/SandboxResources.h>
#include <ui/text/UITextLayout.h>

    #include <aurora/rdg/ClientViewport.h>
    #include <aurora/rdg/DeviceFrameContext.h>
    #include <aurora/renderer/IRenderer.h>
    #include <aurora/rhi/CommandBuffer.h>
    #include <aurora/rhi/Core.h>
    #include <aurora/rhi/Device.h>
    #include <aurora/rhi/Encoder.h>
    #include <aurora/rhi/Image.h>
    #include <aurora/rhi/Instance.h>
    #include <aurora/rhi/Queue.h>
    #include <aurora/rhi/Semaphore.h>
    #include <aurora/rhi/SubmitInfo.h>
    #include <aurora/rhi/SwapChain.h>

#include <algorithm>
#include <core/logger/Logger.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/platform/PlatformBase.h>
#include <framework/window/NativeWindow.h>

static const char *TAG = "EditorRender";

namespace {
    // Viewport content target (TEXTURE presentation placeholder).
    constexpr uint32_t             kViewportWidth     = 320;
    constexpr uint32_t             kViewportHeight    = 180;
    constexpr sky::ui::UITextureId kViewportTextureId = 1;
    // Standalone preview window (WINDOW presentation placeholder).
    constexpr uint32_t kPreviewWidth  = 320;
    constexpr uint32_t kPreviewHeight = 180;
} // namespace

namespace sky::editor {

    using namespace sky::aurora;

    EditorRenderer::EditorRenderer() = default;
    EditorRenderer::~EditorRenderer()
    {
        Shutdown();
    }

    bool EditorRenderer::Init(const std::string &appName, uint32_t inWidth, uint32_t inHeight, API api, bool withPreview, RendererVulkanMode)
    {
        if (Interface<IRenderer>::Get()->GetApi()) {
            LOG_E(TAG, "legacy editor host cannot share an active Renderer session");
            return false;
        }
        shutdownRequested = false;
        width  = inWidth;
        height = inHeight;

        // The standalone preview window is only created in editor mode; the hub
        // (Project Manager) has no preview surface.
        if (withPreview) {
            // Create any window before the RHI instance so backends that bind the
            // surface during instance setup can do so.
            previewWindow.reset(
                NativeWindow::Create(NativeWindow::Descriptor{kPreviewWidth, kPreviewHeight, "SkyEnginePreview", "SkyEngine Preview", nullptr}));
            if (previewWindow == nullptr) {
                LOG_E(TAG, "preview window creation failed");
            }
        }

        Instance::Descriptor desc = {};
        desc.appName              = appName.c_str();
        desc.engineName           = "SkyEngine";
#if SKY_ENABLE_GPU_VALIDATION
        desc.enableDebugLayer = true;
#else
        desc.enableDebugLayer = false;
#endif
        desc.api = api;

        Instance::Get()->Init(desc);
        device = Instance::Get()->GetDevice();
        if (device == nullptr) {
            LOG_E(TAG, "aurora device init failed");
            return false;
        }

        DeviceFrameContextInitInfo frameInfo{};
        frameInfo.inflightNum = 1;
        frameInfo.parallelNum = 1;
        frameContext.reset(device->CreateFrameContext(frameInfo));

        commandPool.reset(device->CreateCommandPool(QueueType::GRAPHICS));
        if (!commandPool || !commandPool->Init()) {
            LOG_E(TAG, "aurora command pool init failed");
            return false;
        }
        commandBuffer = commandPool->Allocate();
        if (commandBuffer == nullptr) {
            LOG_E(TAG, "aurora command buffer allocation failed");
            return false;
        }

        // GUI pipeline lives in UIRender; the renderer only feeds it draw data.
        uiRenderer.Init(device, PixelFormat::BGRA8_UNORM);
#if defined(SKY_BUILD_FREETYPE)
        if (freeTypeFont.LoadFont(SandboxResources::Resolve("fonts/OpenSans-Regular.ttf")) && freeTypeFont.IsReady()) {
            textSystem = std::make_unique<sky::ui::UITextSystem>(&freeTypeFont, &uiRenderer, 256);
            LOG_I(TAG, "UI text: FreeType (OpenSans-Regular)");
        } else {
            LOG_W(TAG, "UI text: could not load font, falling back to builtin provider");
            textSystem = std::make_unique<sky::ui::UITextSystem>(&builtinFont, &uiRenderer, 128);
        }
#else
        textSystem = std::make_unique<sky::ui::UITextSystem>(&builtinFont, &uiRenderer, 128);
#endif

        // Viewport content target (TEXTURE presentation): an offscreen image the
        // scene renderer will draw into; composited as a UI image for now.
        Image::Descriptor targetDesc = {};
        targetDesc.imageType         = ImageType::IMAGE_2D;
        targetDesc.format            = PixelFormat::BGRA8_UNORM;
        targetDesc.extent            = {kViewportWidth, kViewportHeight, 1};
        targetDesc.mipLevels         = 1;
        targetDesc.arrayLayers       = 1;
        targetDesc.samples           = SampleCount::X1;
        targetDesc.usage             = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::SAMPLED;
        targetDesc.memory            = MemoryType::GPU_ONLY;
        contentTarget                = device->CreateImage(targetDesc);
        if (contentTarget != nullptr) {
            uiRenderer.RegisterImage(kViewportTextureId, contentTarget.Get());
        }

        // Standalone preview target (blit source for the WINDOW presentation).
        Image::Descriptor previewDesc = {};
        previewDesc.imageType         = ImageType::IMAGE_2D;
        previewDesc.format            = PixelFormat::BGRA8_UNORM;
        previewDesc.extent            = {kPreviewWidth, kPreviewHeight, 1};
        previewDesc.mipLevels         = 1;
        previewDesc.arrayLayers       = 1;
        previewDesc.samples           = SampleCount::X1;
        previewDesc.usage             = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
        previewDesc.memory            = MemoryType::GPU_ONLY;
        previewTarget                 = device->CreateImage(previewDesc);
        return true;
    }

    void EditorRenderer::PaintUI(uint32_t surfaceId, uint32_t surfaceWidth, uint32_t surfaceHeight)
    {
        // GUI content comes from the editor shell (set via SetGuiSource); the
        // renderer holds no hardcoded UI.
        if (!guiSource) {
            return;
        }
        guiSource(surfaceId, paintContext, surfaceWidth, surfaceHeight);
    }

    void EditorRenderer::Start()
    {
        if (device == nullptr) {
            return;
        }

        void *window = nullptr;
        if (auto *system = Interface<ISystemNotify>::Get()->GetApi()) {
            window = system->GetMainWindowHandle();
        }
        if (window == nullptr) {
            window = Platform::Get()->GetMainWinHandle();
        }
        if (window == nullptr) {
            LOG_E(TAG, "no main window handle; skipping viewport");
            return;
        }

        SwapChain::Descriptor scDesc = {};
        scDesc.window                = window;
        scDesc.width                 = width;
        scDesc.height                = height;
        scDesc.preferredFormat       = PixelFormat::BGRA8_UNORM;
        scDesc.preferredMode         = PresentMode::IMMEDIATE;

        viewport = std::make_unique<ClientViewport>(Name("editor"));
        if (!viewport->Init(device, scDesc)) {
            LOG_E(TAG, "client viewport init failed");
            viewport.reset();
        }

        // Standalone preview window (WINDOW presentation).
        if (previewWindow != nullptr) {
            SwapChain::Descriptor previewSc = {};
            previewSc.window                = previewWindow->GetNativeHandle();
            previewSc.width                 = kPreviewWidth;
            previewSc.height                = kPreviewHeight;
            previewSc.preferredFormat       = PixelFormat::BGRA8_UNORM;
            previewSc.preferredMode         = PresentMode::IMMEDIATE;
            previewViewport                 = std::make_unique<ClientViewport>(Name("editor_preview"));
            if (!previewViewport->Init(device, previewSc)) {
                LOG_E(TAG, "preview viewport init failed");
                previewViewport.reset();
            }
        }
        if (previewViewport != nullptr) {
            previewCommandBuffer = commandPool->Allocate();
        }

        // React to the preview window closing so we stop acquiring it.
        if (previewWindow != nullptr) {
            previewEvents = std::make_unique<WindowEventBinding>(*this, previewWindow.get());
        }
    }

    uint32_t EditorRenderer::CreateSurface(const std::string &panelId, int x, int y, uint32_t w, uint32_t h)
    {
        if (shutdownRequested || device == nullptr || commandPool == nullptr || textSystem == nullptr || w == 0 || h == 0) {
            return 0;
        }

        auto surface     = std::make_unique<FloatingSurface>();
        surface->id      = nextSurfaceId++;
        surface->panelId = panelId;
        surface->x       = static_cast<float>(x);
        surface->y       = static_cast<float>(y);
        surface->w       = w;
        surface->h       = h;

        const std::string name = "SkyEnginePanel" + std::to_string(surface->id);
        surface->window.reset(NativeWindow::Create(NativeWindow::Descriptor{w, h, name.c_str(), panelId.c_str(), nullptr}));
        if (surface->window == nullptr) {
            LOG_E(TAG, "floating window creation failed for panel '%s'", panelId.c_str());
            return 0;
        }
        // Capture the window's DPI once (per-window UI scale).
        surface->dpiScale = surface->window->GetDpiScale();

        SwapChain::Descriptor scDesc = {};
        scDesc.window                = surface->window->GetNativeHandle();
        scDesc.width                 = w;
        scDesc.height                = h;
        scDesc.preferredFormat       = PixelFormat::BGRA8_UNORM;
        scDesc.preferredMode         = PresentMode::IMMEDIATE;
        surface->viewport            = std::make_unique<ClientViewport>(Name("editor_panel"));
        if (!surface->viewport->Init(device, scDesc)) {
            LOG_E(TAG, "floating viewport init failed");
            return 0;
        }
        surface->commandBuffer = commandPool->Allocate();

        surface->uiRenderer = std::make_unique<sky::ui::UIRenderer>();
        surface->uiRenderer->Init(device, PixelFormat::BGRA8_UNORM);
        if (contentTarget != nullptr) {
            surface->uiRenderer->RegisterImage(kViewportTextureId, contentTarget.Get());
        }
        // Share the font atlas so text renders in this window too.
        textSystem->AddRegistry(surface->uiRenderer.get());

        surface->events = std::make_unique<WindowEventBinding>(*this, surface->window.get());
        surfaces.push_back(std::move(surface));
        return surfaces.back()->id;
    }

    void EditorRenderer::DestroySurface(uint32_t id)
    {
        for (auto &surface : surfaces) {
            if (surface->id == id) {
                RequestSurfaceClose(*surface, false);
                return;
            }
        }
    }

    void EditorRenderer::RequestSurfaceClose(FloatingSurface &surface, bool notifyClosed)
    {
        surface.closed       = true;
        surface.notifyClosed = surface.notifyClosed || notifyClosed;
        surface.events.reset();
        if (textSystem != nullptr && surface.uiRenderer != nullptr) {
            textSystem->RemoveRegistry(surface.uiRenderer.get());
        }
    }

    void EditorRenderer::RetireClosedSurfaces()
    {
        const bool hasClosedSurface = std::any_of(surfaces.begin(), surfaces.end(), [](const auto &surface) { return surface->closed; });
        if (!previewClosed && !hasClosedSurface) {
            return;
        }
        // WaitIdle covers rendering AND presentation queues. The next frame's
        // BeginFrame fence wait would be too late for swapchains released here.
        if (device != nullptr) {
            device->WaitIdle();
        }
        if (previewClosed) {
            previewEvents.reset();
            previewViewport.reset();
            previewCommandBuffer = nullptr;
            previewWindow.reset();
            previewClosed = false;
        }

        std::vector<std::string> closedPanels;
        for (auto it = surfaces.begin(); it != surfaces.end();) {
            if (!(*it)->closed) {
                ++it;
                continue;
            }
            if ((*it)->notifyClosed) {
                closedPanels.push_back((*it)->panelId);
            }
            if ((*it)->uiRenderer != nullptr) {
                (*it)->uiRenderer->Shutdown();
            }
            it = surfaces.erase(it);
        }
        // Re-docking can create/close other surfaces. Notify after iteration and
        // resource release so callbacks cannot invalidate the retirement loop.
        const auto callback = surfaceClosed;
        for (const auto &panelId : closedPanels) {
            if (shutdownRequested) {
                break;
            }
            if (callback) {
                callback(panelId);
            }
        }
    }

    uint32_t EditorRenderer::SurfaceIdForWindow(const sky::NativeWindow *window) const
    {
        for (const auto &surface : surfaces) {
            if (surface->window.get() == window) {
                return surface->id;
            }
        }
        return 0;
    }

    float EditorRenderer::SurfaceDpiScale(uint32_t id) const
    {
        for (const auto &surface : surfaces) {
            if (surface->id == id) {
                return surface->dpiScale;
            }
        }
        return 1.0f;
    }

    void EditorRenderer::RecordUiPass(sky::aurora::CommandBuffer  *cmd,
                                      sky::aurora::Image          *backbuffer,
                                      uint32_t                     w,
                                      uint32_t                     h,
                                      sky::ui::UIRenderer         &uiRenderer,
                                      sky::ui::UIPaintContext     &paintContext,
                                      const std::function<void()> &paint)
    {
        paint();
        uiRenderer.UpdateDrawData(paintContext.GetDrawData(), w, h);
        uiRenderer.EnsureTextureReady(cmd);
        {
            auto          encoder  = cmd->CreateGraphicsEncoder();
            RenderingInfo info     = {};
            info.renderArea        = {{0, 0}, Extent2D{w, h}};
            info.numColors         = 1;
            info.colors[0].image   = backbuffer;
            info.colors[0].loadOp  = LoadOp::LOAD;
            info.colors[0].storeOp = StoreOp::STORE;
            encoder->BeginRendering(info);
            uiRenderer.Render(encoder.get(), paintContext.GetDrawData(), w, h);
            encoder->EndRendering();
        }
        {
            BarrierInfo barrier{};
            barrier.srcStage = PipelineStageBit::COLOR_OUTPUT;
            barrier.dstStage = PipelineStageBit::BOTTOM;
            ImageBarrierInfo imageBarrier{};
            imageBarrier.image     = backbuffer;
            imageBarrier.srcAccess = AccessFlagBit::RTV;
            imageBarrier.dstAccess = AccessFlagBit::PRESENT;
            imageBarrier.oldLayout = ImageLayout::COLOR_ATTACHMENT;
            imageBarrier.newLayout = ImageLayout::PRESENT;
            barrier.imageBarriers.push_back(imageBarrier);
            cmd->PipelineBarrier(barrier);
        }
    }

    void EditorRenderer::RenderSurface(FloatingSurface &surface, EditorFrameSubmission &frame)
    {
        if (surface.closed || surface.viewport == nullptr || surface.commandBuffer == nullptr || surface.uiRenderer == nullptr) {
            return;
        }
        if (!frame.Acquire(*surface.viewport)) {
            return;
        }
        const Extent2D extent     = surface.viewport->GetExtent();
        Image         *backbuffer = surface.viewport->GetBackbuffer();

        surface.commandBuffer->Begin();
        {
            BarrierInfo barrier{};
            barrier.srcStage = PipelineStageBit::TOP;
            barrier.dstStage = PipelineStageBit::COLOR_OUTPUT;
            ImageBarrierInfo imageBarrier{};
            imageBarrier.image     = backbuffer;
            imageBarrier.srcAccess = AccessFlagBit::NONE;
            imageBarrier.dstAccess = AccessFlagBit::RTV;
            imageBarrier.oldLayout = ImageLayout::UNDEFINED;
            imageBarrier.newLayout = ImageLayout::COLOR_ATTACHMENT;
            barrier.imageBarriers.push_back(imageBarrier);
            surface.commandBuffer->PipelineBarrier(barrier);
        }
        {
            auto          encoder     = surface.commandBuffer->CreateGraphicsEncoder();
            RenderingInfo info        = {};
            info.renderArea           = {{0, 0}, extent};
            info.numColors            = 1;
            info.colors[0].image      = backbuffer;
            info.colors[0].loadOp     = LoadOp::CLEAR;
            info.colors[0].storeOp    = StoreOp::STORE;
            info.colors[0].clearValue = ClearValue(0.05f, 0.06f, 0.09f, 1.0f);
            encoder->BeginRendering(info);
            encoder->EndRendering();
        }

        RecordUiPass(surface.commandBuffer, backbuffer, extent.width, extent.height, *surface.uiRenderer, surface.paintContext,
                     [this, &surface, extent]() {
                         if (guiSource) {
                             guiSource(surface.id, surface.paintContext, extent.width, extent.height);
                         }
                     });
        surface.commandBuffer->End();
        frame.AddCommandBuffer(surface.commandBuffer);
    }

    void EditorRenderer::OnWindowClose(const sky::NativeWindow *window)
    {
        if (previewWindow != nullptr && window == previewWindow.get()) {
            previewClosed = true;
            previewEvents.reset();
            return;
        }
        for (auto &surface : surfaces) {
            if (surface->window.get() == window) {
                RequestSurfaceClose(*surface, true);
                return;
            }
        }
    }

    void EditorRenderer::OnWindowMove(const sky::WindowMoveEvent &event)
    {
        for (auto &surface : surfaces) {
            if (surface->window != nullptr && surface->window->GetWinId() == event.winID) {
                surface->x = static_cast<float>(event.x);
                surface->y = static_cast<float>(event.y);
                if (surfaceGeometryChanged) {
                    surfaceGeometryChanged(surface->panelId, surface->x, surface->y, static_cast<float>(surface->w), static_cast<float>(surface->h));
                }
                return;
            }
        }
    }

    void EditorRenderer::OnWindowResize(const sky::WindowResizeEvent &event)
    {
        for (auto &surface : surfaces) {
            if (surface->window != nullptr && surface->window->GetWinId() == event.winID) {
                surface->w = event.width;
                surface->h = event.height;
                if (surfaceGeometryChanged) {
                    surfaceGeometryChanged(surface->panelId, surface->x, surface->y, static_cast<float>(surface->w), static_cast<float>(surface->h));
                }
                return;
            }
        }
    }

    void EditorRenderer::Tick(float /*delta*/)
    {
        // A close requested by guiSource must not destroy an object referenced
        // by the current frame's command buffers or activeSurfaces snapshot.
        if (frameActive) {
            return;
        }
        RetireClosedSurfaces();
        if (shutdownRequested) {
            Shutdown();
            return;
        }
        frameActive = true;
        RenderFrame();
        frameActive = false;
        if (shutdownRequested) {
            Shutdown();
        } else {
            RetireClosedSurfaces();
        }
    }

    void EditorRenderer::RenderFrame()
    {
        if (device == nullptr || frameContext == nullptr || commandBuffer == nullptr || viewport == nullptr) {
            return;
        }

        // BeginFrame waits before any viewport can resize its old swapchain.
        // The scope signals its reset fence even when acquire/rendering skips.
        EditorFrameSubmission frame(*frameContext, *device->GetQueue(QueueType::GRAPHICS));
        // BeginFrame has waited for the previous single in-flight submission.
        // Resource generations removed during event dispatch/paint can now die.
        uiRenderer.CollectRetiredTextures();
        for (auto &surface : surfaces) {
            if (!surface->closed && surface->uiRenderer != nullptr) {
                surface->uiRenderer->CollectRetiredTextures();
            }
        }
        if (!frame.Acquire(*viewport)) {
            return;
        }
        const Extent2D extent     = viewport->GetExtent();
        Image         *backbuffer = viewport->GetBackbuffer();

        commandBuffer->Begin();
        {
            BarrierInfo barrier{};
            barrier.srcStage = PipelineStageBit::TOP;
            barrier.dstStage = PipelineStageBit::COLOR_OUTPUT;
            ImageBarrierInfo imageBarrier{};
            imageBarrier.image     = backbuffer;
            imageBarrier.subRange  = ImageSubRange{};
            imageBarrier.srcAccess = AccessFlagBit::NONE;
            imageBarrier.dstAccess = AccessFlagBit::RTV;
            imageBarrier.oldLayout = ImageLayout::UNDEFINED;
            imageBarrier.newLayout = ImageLayout::COLOR_ATTACHMENT;
            barrier.imageBarriers.push_back(imageBarrier);
            commandBuffer->PipelineBarrier(barrier);
        }
        {
            // Scene pass (placeholder): clear the backbuffer.
            auto          encoder     = commandBuffer->CreateGraphicsEncoder();
            RenderingInfo info        = {};
            info.renderArea           = {{0, 0}, extent};
            info.numColors            = 1;
            info.colors[0].image      = backbuffer;
            info.colors[0].loadOp     = LoadOp::CLEAR;
            info.colors[0].storeOp    = StoreOp::STORE;
            info.colors[0].clearValue = ClearValue(0.05f, 0.06f, 0.09f, 1.0f);
            encoder->BeginRendering(info);
            encoder->EndRendering();
        }

        // Render the viewport content target (placeholder "scene") and put it in
        // SHADER_READ_ONLY so the UI pass can sample it.
        if (contentTarget != nullptr) {
            BarrierInfo toColor{};
            toColor.srcStage = PipelineStageBit::TOP;
            toColor.dstStage = PipelineStageBit::COLOR_OUTPUT;
            ImageBarrierInfo colorBarrier{};
            colorBarrier.image     = contentTarget.Get();
            colorBarrier.srcAccess = AccessFlagBit::NONE;
            colorBarrier.dstAccess = AccessFlagBit::RTV;
            colorBarrier.oldLayout = ImageLayout::UNDEFINED;
            colorBarrier.newLayout = ImageLayout::COLOR_ATTACHMENT;
            toColor.imageBarriers.push_back(colorBarrier);
            commandBuffer->PipelineBarrier(toColor);
            {
                auto          targetEncoder = commandBuffer->CreateGraphicsEncoder();
                RenderingInfo info          = {};
                info.renderArea             = {{0, 0}, Extent2D{kViewportWidth, kViewportHeight}};
                info.numColors              = 1;
                info.colors[0].image        = contentTarget.Get();
                info.colors[0].loadOp       = LoadOp::CLEAR;
                info.colors[0].storeOp      = StoreOp::STORE;
                info.colors[0].clearValue   = ClearValue(0.10f, 0.45f, 0.28f, 1.0f);
                targetEncoder->BeginRendering(info);
                targetEncoder->EndRendering();
            }
            BarrierInfo toRead{};
            toRead.srcStage = PipelineStageBit::COLOR_OUTPUT;
            toRead.dstStage = PipelineStageBit::FRAGMENT_SHADER;
            ImageBarrierInfo readBarrier{};
            readBarrier.image     = contentTarget.Get();
            readBarrier.srcAccess = AccessFlagBit::RTV;
            readBarrier.dstAccess = AccessFlagBit::SRV;
            readBarrier.oldLayout = ImageLayout::COLOR_ATTACHMENT;
            readBarrier.newLayout = ImageLayout::SHADER_READ_ONLY;
            toRead.imageBarriers.push_back(readBarrier);
            commandBuffer->PipelineBarrier(toRead);
        }

        RecordUiPass(commandBuffer, backbuffer, extent.width, extent.height, uiRenderer, paintContext,
                     [this, extent]() { PaintUI(0, extent.width, extent.height); });
        commandBuffer->End();
        frame.AddCommandBuffer(commandBuffer);

        // Standalone preview window (WINDOW presentation, scheme C): render its
        // target and blit into its swapchain, recorded into a second command
        // buffer submitted together with the main one.
        if (!shutdownRequested && !previewClosed && previewViewport != nullptr && previewCommandBuffer != nullptr && previewTarget != nullptr &&
            frame.Acquire(*previewViewport)) {
            const Extent2D previewExtent     = previewViewport->GetExtent();
            Image         *previewBackbuffer = previewViewport->GetBackbuffer();

            previewCommandBuffer->Begin();
            {
                BarrierInfo barrier{};
                barrier.srcStage = PipelineStageBit::TOP;
                barrier.dstStage = PipelineStageBit::COLOR_OUTPUT;
                ImageBarrierInfo target{};
                target.image     = previewTarget.Get();
                target.srcAccess = AccessFlagBit::NONE;
                target.dstAccess = AccessFlagBit::RTV;
                target.oldLayout = ImageLayout::UNDEFINED;
                target.newLayout = ImageLayout::COLOR_ATTACHMENT;
                barrier.imageBarriers.push_back(target);
                previewCommandBuffer->PipelineBarrier(barrier);
            }
            {
                auto          encoder     = previewCommandBuffer->CreateGraphicsEncoder();
                RenderingInfo info        = {};
                info.renderArea           = {{0, 0}, Extent2D{kPreviewWidth, kPreviewHeight}};
                info.numColors            = 1;
                info.colors[0].image      = previewTarget.Get();
                info.colors[0].loadOp     = LoadOp::CLEAR;
                info.colors[0].storeOp    = StoreOp::STORE;
                info.colors[0].clearValue = ClearValue(0.15f, 0.35f, 0.75f, 1.0f);
                encoder->BeginRendering(info);
                encoder->EndRendering();
            }
            {
                BarrierInfo barrier{};
                barrier.srcStage = PipelineStageBit::COLOR_OUTPUT;
                barrier.dstStage = PipelineStageBit::TRANSFER;
                ImageBarrierInfo target{};
                target.image     = previewTarget.Get();
                target.srcAccess = AccessFlagBit::RTV;
                target.dstAccess = AccessFlagBit::COPY_SRC;
                target.oldLayout = ImageLayout::COLOR_ATTACHMENT;
                target.newLayout = ImageLayout::TRANSFER_SRC;
                barrier.imageBarriers.push_back(target);
                if (previewBackbuffer != nullptr) {
                    ImageBarrierInfo destination{};
                    destination.image     = previewBackbuffer;
                    destination.srcAccess = AccessFlagBit::NONE;
                    destination.dstAccess = AccessFlagBit::COPY_DST;
                    destination.oldLayout = ImageLayout::UNDEFINED;
                    destination.newLayout = ImageLayout::TRANSFER_DST;
                    barrier.imageBarriers.push_back(destination);
                }
                previewCommandBuffer->PipelineBarrier(barrier);
            }
            if (previewBackbuffer != nullptr && previewExtent.width > 0 && previewExtent.height > 0) {
                auto     blit        = previewCommandBuffer->CreateBlitEncoder();
                BlitInfo region      = {};
                region.srcOffsets[0] = {0, 0, 0};
                region.srcOffsets[1] = {static_cast<int32_t>(kPreviewWidth), static_cast<int32_t>(kPreviewHeight), 1};
                region.dstOffsets[0] = {0, 0, 0};
                region.dstOffsets[1] = {static_cast<int32_t>(kPreviewWidth), static_cast<int32_t>(kPreviewHeight), 1};
                blit->BlitImage(previewTarget.Get(), previewBackbuffer, {region}, Filter::NEAREST);

                BarrierInfo toPresent{};
                toPresent.srcStage = PipelineStageBit::TRANSFER;
                toPresent.dstStage = PipelineStageBit::BOTTOM;
                ImageBarrierInfo present{};
                present.image     = previewBackbuffer;
                present.srcAccess = AccessFlagBit::COPY_DST;
                present.dstAccess = AccessFlagBit::PRESENT;
                present.oldLayout = ImageLayout::TRANSFER_DST;
                present.newLayout = ImageLayout::PRESENT;
                toPresent.imageBarriers.push_back(present);
                previewCommandBuffer->PipelineBarrier(toPresent);
            }
            previewCommandBuffer->End();
            frame.AddCommandBuffer(previewCommandBuffer);
        }

        // Record floating panel windows (each into its own command buffer).
        std::vector<FloatingSurface *> frameSurfaces;
        for (auto &surface : surfaces) {
            frameSurfaces.push_back(surface.get());
        }
        for (FloatingSurface *surface : frameSurfaces) {
            RenderSurface(*surface, frame);
        }
        // frame submits all recorded work (or a fence-only batch), signals each
        // acquired viewport, presents it, then ends the frame on every path.
    }

    void EditorRenderer::Shutdown()
    {
        shutdownRequested = true;
        previewEvents.reset();
        for (auto &surface : surfaces) {
            RequestSurfaceClose(*surface, false);
        }
        // A GUI callback may request shutdown while recording. Keep all frame
        // resources alive until that frame has submitted and presented.
        if (frameActive) {
            return;
        }
        if (device != nullptr) {
            device->WaitIdle();
        }
        for (auto &surface : surfaces) {
            if (surface->uiRenderer != nullptr) {
                surface->uiRenderer->Shutdown();
            }
        }
        surfaces.clear();

        textSystem.reset();
        uiRenderer.Shutdown();
        previewCommandBuffer = nullptr;
        previewViewport.reset();
        previewWindow.reset();
        previewTarget.Reset(nullptr);
        previewClosed = false;
        viewport.reset();
        contentTarget.Reset(nullptr);
        commandBuffer = nullptr;
        commandPool.reset();
        frameContext.reset();
        device = nullptr;
    }

} // namespace sky::editor

#endif
