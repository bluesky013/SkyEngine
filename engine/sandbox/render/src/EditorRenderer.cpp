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

#include <editor/core/resource/SandboxResources.h>
#include <ui/text/UITextLayout.h>

#include <aurora/rdg/ClientViewport.h>
#include <aurora/rdg/RenderDeviceExclusive.h>
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

#include <core/logger/Logger.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/platform/PlatformBase.h>
#include <framework/window/NativeWindow.h>

static const char *TAG = "EditorRender";

namespace {
    // Viewport content target (TEXTURE presentation placeholder).
    constexpr uint32_t kViewportWidth  = 320;
    constexpr uint32_t kViewportHeight = 180;
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
        sky::Event<sky::IWindowEvent>::DisConnect(this);
    }

    bool EditorRenderer::Init(const std::string &appName, uint32_t inWidth, uint32_t inHeight, API api,
                              bool withPreview)
    {
        width  = inWidth;
        height = inHeight;

        // The standalone preview window is only created in editor mode; the hub
        // (Project Manager) has no preview surface.
        if (withPreview) {
            // Create any window before the RHI instance so backends that bind the
            // surface during instance setup can do so.
            previewWindow.reset(NativeWindow::Create(
                NativeWindow::Descriptor{kPreviewWidth, kPreviewHeight, "SkyEnginePreview", "SkyEngine Preview", nullptr}));
            if (previewWindow == nullptr) {
                LOG_E(TAG, "preview window creation failed");
            }
        }

        Instance::Descriptor desc = {};
        desc.appName              = appName.c_str();
        desc.engineName           = "SkyEngine";
#if defined(_DEBUG)
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
        targetDesc.imageType   = ImageType::IMAGE_2D;
        targetDesc.format      = PixelFormat::BGRA8_UNORM;
        targetDesc.extent      = {kViewportWidth, kViewportHeight, 1};
        targetDesc.mipLevels   = 1;
        targetDesc.arrayLayers = 1;
        targetDesc.samples     = SampleCount::X1;
        targetDesc.usage       = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::SAMPLED;
        targetDesc.memory      = MemoryType::GPU_ONLY;
        contentTarget = device->CreateImage(targetDesc);
        if (contentTarget != nullptr) {
            uiRenderer.RegisterImage(kViewportTextureId, contentTarget.Get());
        }

        // Standalone preview target (blit source for the WINDOW presentation).
        Image::Descriptor previewDesc = {};
        previewDesc.imageType   = ImageType::IMAGE_2D;
        previewDesc.format      = PixelFormat::BGRA8_UNORM;
        previewDesc.extent      = {kPreviewWidth, kPreviewHeight, 1};
        previewDesc.mipLevels   = 1;
        previewDesc.arrayLayers = 1;
        previewDesc.samples     = SampleCount::X1;
        previewDesc.usage       = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
        previewDesc.memory      = MemoryType::GPU_ONLY;
        previewTarget = device->CreateImage(previewDesc);
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
            previewSc.window          = previewWindow->GetNativeHandle();
            previewSc.width           = kPreviewWidth;
            previewSc.height          = kPreviewHeight;
            previewSc.preferredFormat = PixelFormat::BGRA8_UNORM;
            previewSc.preferredMode   = PresentMode::IMMEDIATE;
            previewViewport = std::make_unique<ClientViewport>(Name("editor_preview"));
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
            sky::Event<sky::IWindowEvent>::Connect(previewWindow.get(), this);
        }
    }

    uint32_t EditorRenderer::CreateSurface(const std::string &panelId, int x, int y, uint32_t w, uint32_t h)
    {
        if (device == nullptr || commandPool == nullptr || textSystem == nullptr || w == 0 || h == 0) {
            return 0;
        }

        auto surface = std::make_unique<FloatingSurface>();
        surface->id = nextSurfaceId++;
        surface->panelId = panelId;
        surface->x = static_cast<float>(x);
        surface->y = static_cast<float>(y);
        surface->w = w;
        surface->h = h;

        const std::string name = "SkyEnginePanel" + std::to_string(surface->id);
        surface->window.reset(NativeWindow::Create(NativeWindow::Descriptor{w, h, name.c_str(), panelId.c_str(), nullptr}));
        if (surface->window == nullptr) {
            LOG_E(TAG, "floating window creation failed for panel '%s'", panelId.c_str());
            return 0;
        }
        // Capture the window's DPI once (per-window UI scale).
        surface->dpiScale = surface->window->GetDpiScale();

        SwapChain::Descriptor scDesc = {};
        scDesc.window          = surface->window->GetNativeHandle();
        scDesc.width           = w;
        scDesc.height          = h;
        scDesc.preferredFormat = PixelFormat::BGRA8_UNORM;
        scDesc.preferredMode   = PresentMode::IMMEDIATE;
        surface->viewport = std::make_unique<ClientViewport>(Name("editor_panel"));
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

        sky::Event<sky::IWindowEvent>::Connect(surface->window.get(), this);
        surfaces.push_back(std::move(surface));
        return surfaces.back()->id;
    }

    void EditorRenderer::DestroySurface(uint32_t id)
    {
        for (auto it = surfaces.begin(); it != surfaces.end(); ++it) {
            if ((*it)->id != id) {
                continue;
            }
            if ((*it)->uiRenderer != nullptr) {
                (*it)->uiRenderer->Shutdown();
            }
            surfaces.erase(it);
            return;
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

    void EditorRenderer::RecordUiPass(sky::aurora::CommandBuffer *cmd, sky::aurora::Image *backbuffer, uint32_t w,
                                      uint32_t h, sky::ui::UIRenderer &uiRenderer,
                                      sky::ui::UIPaintContext &paintContext, const std::function<void()> &paint)
    {
        paint();
        uiRenderer.UpdateDrawData(paintContext.GetDrawData(), w, h);
        uiRenderer.EnsureTextureReady(cmd);
        {
            auto encoder = cmd->CreateGraphicsEncoder();
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

    bool EditorRenderer::RenderSurface(FloatingSurface &surface)
    {
        if (surface.viewport == nullptr || surface.commandBuffer == nullptr || surface.uiRenderer == nullptr) {
            return false;
        }
        if (!surface.viewport->Begin() || !surface.viewport->Acquire()) {
            return false;
        }
        const Extent2D extent = surface.viewport->GetExtent();
        Image *backbuffer = surface.viewport->GetBackbuffer();
        if (backbuffer == nullptr || extent.width == 0 || extent.height == 0) {
            surface.viewport->Release();
            return false;
        }

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
            auto encoder = surface.commandBuffer->CreateGraphicsEncoder();
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

        RecordUiPass(surface.commandBuffer, backbuffer, extent.width, extent.height, *surface.uiRenderer,
                     surface.paintContext, [this, &surface, extent]() {
                         if (guiSource) {
                             guiSource(surface.id, surface.paintContext, extent.width, extent.height);
                         }
                     });
        surface.commandBuffer->End();
        return true;
    }

    void EditorRenderer::OnWindowClose(const sky::NativeWindow *window)
    {
        if (previewWindow != nullptr && window == previewWindow.get()) {
            previewClosed = true;
            return;
        }
        for (auto &surface : surfaces) {
            if (surface->window.get() == window) {
                surface->closed = true;
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
                    surfaceGeometryChanged(surface->panelId, surface->x, surface->y, static_cast<float>(surface->w),
                                           static_cast<float>(surface->h));
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
                    surfaceGeometryChanged(surface->panelId, surface->x, surface->y, static_cast<float>(surface->w),
                                           static_cast<float>(surface->h));
                }
                return;
            }
        }
    }

    void EditorRenderer::Tick(float /*delta*/)
    {
        // Drop a closed preview window outside its message handler: its HWND is
        // already gone, so release the swapchain and stop acquiring it.
        if (previewClosed) {
            previewViewport.reset();
            previewCommandBuffer = nullptr;
            previewWindow.reset();
            previewClosed = false;
        }

        // Drop floating windows the user closed, and report back so the shell can
        // re-dock the panel.
        for (auto it = surfaces.begin(); it != surfaces.end();) {
            if ((*it)->closed) {
                const std::string panelId = (*it)->panelId;
                if ((*it)->uiRenderer != nullptr) {
                    (*it)->uiRenderer->Shutdown();
                }
                it = surfaces.erase(it);
                if (surfaceClosed) {
                    surfaceClosed(panelId);
                }
            } else {
                ++it;
            }
        }

        if (device == nullptr || frameContext == nullptr || commandBuffer == nullptr || viewport == nullptr) {
            return;
        }

        frameContext->BeginFrame();
        if (!viewport->Begin()) {
            frameContext->EndFrame();
            return;
        }

        // Check the surface size before acquiring so a minimized/zero-size window
        // never acquires an image it cannot present.
        const Extent2D extent = viewport->GetExtent();
        if (extent.width == 0 || extent.height == 0) {
            frameContext->EndFrame();
            return;
        }
        if (!viewport->Acquire()) {
            frameContext->EndFrame();
            return;
        }

        Image *backbuffer = viewport->GetBackbuffer();
        if (backbuffer == nullptr) {
            viewport->Release(); // acquired but unusable; present to keep the queue moving
            frameContext->EndFrame();
            return;
        }

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
            auto encoder = commandBuffer->CreateGraphicsEncoder();
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
                auto targetEncoder = commandBuffer->CreateGraphicsEncoder();
                RenderingInfo info        = {};
                info.renderArea           = {{0, 0}, Extent2D{kViewportWidth, kViewportHeight}};
                info.numColors            = 1;
                info.colors[0].image      = contentTarget.Get();
                info.colors[0].loadOp     = LoadOp::CLEAR;
                info.colors[0].storeOp    = StoreOp::STORE;
                info.colors[0].clearValue = ClearValue(0.10f, 0.45f, 0.28f, 1.0f);
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

        // Standalone preview window (WINDOW presentation, scheme C): render its
        // target and blit into its swapchain, recorded into a second command
        // buffer submitted together with the main one.
        bool hasPreview = false;
        if (previewViewport != nullptr && previewCommandBuffer != nullptr && previewTarget != nullptr &&
            previewViewport->Begin() && previewViewport->Acquire()) {
            hasPreview = true;
            const Extent2D previewExtent = previewViewport->GetExtent();
            Image *previewBackbuffer = previewViewport->GetBackbuffer();

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
                auto encoder = previewCommandBuffer->CreateGraphicsEncoder();
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
                auto blit = previewCommandBuffer->CreateBlitEncoder();
                BlitInfo region = {};
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
        }

        // Record floating panel windows (each into its own command buffer).
        std::vector<FloatingSurface *> activeSurfaces;
        for (auto &surface : surfaces) {
            if (RenderSurface(*surface)) {
                activeSurfaces.push_back(surface.get());
            }
        }

        SubmitInfo submit = {};
        submit.commandBuffers.push_back(commandBuffer);

        SemaphoreSubmitInfo mainWait = {};
        mainWait.semaphore           = viewport->GetAcquireSemaphore();
        mainWait.stageMask           = PipelineStageBit::COLOR_OUTPUT;
        submit.waitSemaphores.push_back(mainWait);

        SemaphoreSubmitInfo mainSignal = {};
        mainSignal.semaphore           = viewport->GetRenderDoneSemaphore();
        mainSignal.stageMask           = PipelineStageBit::BOTTOM;
        submit.signalSemaphores.push_back(mainSignal);

        if (hasPreview) {
            submit.commandBuffers.push_back(previewCommandBuffer);

            SemaphoreSubmitInfo previewWait = {};
            previewWait.semaphore           = previewViewport->GetAcquireSemaphore();
            previewWait.stageMask           = PipelineStageBit::COLOR_OUTPUT;
            submit.waitSemaphores.push_back(previewWait);

            SemaphoreSubmitInfo previewSignal = {};
            previewSignal.semaphore           = previewViewport->GetRenderDoneSemaphore();
            previewSignal.stageMask           = PipelineStageBit::BOTTOM;
            submit.signalSemaphores.push_back(previewSignal);
        }

        for (FloatingSurface *surface : activeSurfaces) {
            submit.commandBuffers.push_back(surface->commandBuffer);

            SemaphoreSubmitInfo surfaceWait = {};
            surfaceWait.semaphore = surface->viewport->GetAcquireSemaphore();
            surfaceWait.stageMask = PipelineStageBit::COLOR_OUTPUT;
            submit.waitSemaphores.push_back(surfaceWait);

            SemaphoreSubmitInfo surfaceSignal = {};
            surfaceSignal.semaphore = surface->viewport->GetRenderDoneSemaphore();
            surfaceSignal.stageMask = PipelineStageBit::BOTTOM;
            submit.signalSemaphores.push_back(surfaceSignal);
        }

        submit.fence = frameContext->GetFrameFence();
        device->GetQueue(QueueType::GRAPHICS)->Submit(submit);

        viewport->Release();
        if (hasPreview) {
            previewViewport->Release();
        }
        for (FloatingSurface *surface : activeSurfaces) {
            surface->viewport->Release();
        }
        frameContext->EndFrame();
    }

    void EditorRenderer::Shutdown()
    {
        sky::Event<sky::IWindowEvent>::DisConnect(this);

        if (device != nullptr) {
            device->WaitIdle();
        }
        for (auto &surface : surfaces) {
            if (surface->uiRenderer != nullptr) {
                surface->uiRenderer->Shutdown();
            }
        }
        surfaces.clear();

        uiRenderer.Shutdown();
        previewCommandBuffer = nullptr;
        previewViewport.reset();
        previewWindow.reset();
        previewTarget.Reset(nullptr);
        viewport.reset();
        commandBuffer = nullptr;
        commandPool.reset();
        frameContext.reset();
        device = nullptr;
    }

} // namespace sky::editor
