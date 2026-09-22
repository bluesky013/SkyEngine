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
    EditorRenderer::~EditorRenderer() = default;

    bool EditorRenderer::Init(const std::string &appName, uint32_t inWidth, uint32_t inHeight)
    {
        width  = inWidth;
        height = inHeight;

        // Create the preview window BEFORE the RHI instance: SDL only creates
        // Vulkan-capable windows while it still owns the instance setup.
        previewWindow.reset(NativeWindow::Create(
            NativeWindow::Descriptor{kPreviewWidth, kPreviewHeight, "SkyEnginePreview", "SkyEngine Preview", nullptr}));
        if (previewWindow == nullptr) {
            LOG_E(TAG, "preview window creation failed");
        }

        Instance::Descriptor desc = {};
        desc.appName              = appName.c_str();
        desc.engineName           = "SkyEngine";
#if defined(_DEBUG)
        desc.enableDebugLayer = true;
#else
        desc.enableDebugLayer = false;
#endif
        desc.api = API::DEFAULT;

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

    void EditorRenderer::PaintUI(uint32_t surfaceWidth, uint32_t surfaceHeight)
    {
        // A small real UI painted with sky::ui: a top bar, a left panel and a
        // semi-transparent overlay. Pixel space; UIRenderer converts to NDC.
        const float w = static_cast<float>(surfaceWidth);
        const float h = static_cast<float>(surfaceHeight);

        paintContext.Begin(sky::ui::UIRect{0.0f, 0.0f, w, h});
        // UIVertex.color is packed ABGR (0xAABBGGRR): the RGBA8 vertex attribute
        // reads the bytes as R,G,B,A, so R must be in the low byte.
        paintContext.AddRect(sky::ui::UIRect{0.0f, 0.0f, w, 40.0f}, 0xFF3A2F2A);          // top bar  rgb(2A,2F,3A)
        paintContext.AddRect(sky::ui::UIRect{0.0f, 40.0f, 240.0f, h}, 0xFF201C18);       // left panel rgb(18,1C,20)
        paintContext.AddRect(sky::ui::UIRect{24.0f, 24.0f, 180.0f, 56.0f}, 0xFF3399E5);  // accent  rgb(E5,99,33)
        paintContext.AddRect(sky::ui::UIRect{280.0f, 80.0f, 640.0f, 260.0f}, 0x803399E5); // overlay rgb(E5,99,33) a=0x80

        // Viewport content target (TEXTURE presentation) composited as a UI image.
        const sky::ui::UIRect viewportRect{340.0f, 300.0f, 340.0f + kViewportWidth, 300.0f + kViewportHeight};
        paintContext.AddRect(
            sky::ui::UIRect{viewportRect.left - 2.0f, viewportRect.top - 2.0f, viewportRect.right + 2.0f,
                            viewportRect.bottom + 2.0f},
            0xFF8899AA);                                                              // border
        paintContext.AddTexturedQuad(viewportRect, sky::ui::UIRect{0.0f, 0.0f, 1.0f, 1.0f}, kViewportTextureId,
                                     0xFFFFFFFF);                                     // content

        // Text (glyph atlas through the UIRenderer's IUITextureRegistry). Colors
        // are packed ABGR (0xAABBGGRR).
        if (textSystem != nullptr) {
            sky::ui::UIFontAtlas &atlas = textSystem->GetAtlas();
            sky::ui::UITextLayout::Emit(paintContext, "SkyEngine Editor", 18, 280.0f, 10.0f, 0xFFFFFFFF, atlas);
            sky::ui::UITextLayout::Emit(paintContext, "Outliner", 14, 12.0f, 90.0f, 0xFFDEC4B0, atlas);
            sky::ui::UITextLayout::Emit(paintContext, "Inspector", 14, 12.0f, 120.0f, 0xFFDEC4B0, atlas);
            sky::ui::UITextLayout::Emit(paintContext, "Viewport", 16, 348.0f, 276.0f, 0xFFE0E0E0, atlas);
        }
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
    }

    void EditorRenderer::Tick(float /*delta*/)
    {
        if (device == nullptr || frameContext == nullptr || commandBuffer == nullptr || viewport == nullptr) {
            return;
        }

        frameContext->BeginFrame();
        if (!viewport->Begin() || !viewport->Acquire()) {
            frameContext->EndFrame();
            return;
        }

        Image       *backbuffer = viewport->GetBackbuffer();
        const Extent2D extent   = viewport->GetExtent();
        if (backbuffer == nullptr || extent.width == 0 || extent.height == 0) {
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

        PaintUI(extent.width, extent.height);
        uiRenderer.UpdateDrawData(paintContext.GetDrawData(), extent.width, extent.height);
        // Upload/transition textures AFTER PaintUI: glyph pages are created lazily
        // during painting, so they must be transitioned before this frame samples them.
        uiRenderer.EnsureTextureReady(commandBuffer);
        {
            // UI pass: UIRenderer draws the sky::ui draw data over the scene.
            auto encoder = commandBuffer->CreateGraphicsEncoder();
            RenderingInfo info     = {};
            info.renderArea        = {{0, 0}, extent};
            info.numColors         = 1;
            info.colors[0].image    = backbuffer;
            info.colors[0].loadOp   = LoadOp::LOAD;
            info.colors[0].storeOp  = StoreOp::STORE;
            encoder->BeginRendering(info);
            uiRenderer.Render(encoder.get(), paintContext.GetDrawData(), extent.width, extent.height);
            encoder->EndRendering();
        }
        {
            BarrierInfo barrier{};
            barrier.srcStage = PipelineStageBit::COLOR_OUTPUT;
            barrier.dstStage = PipelineStageBit::BOTTOM;
            ImageBarrierInfo imageBarrier{};
            imageBarrier.image     = backbuffer;
            imageBarrier.subRange  = ImageSubRange{};
            imageBarrier.srcAccess = AccessFlagBit::RTV;
            imageBarrier.dstAccess = AccessFlagBit::PRESENT;
            imageBarrier.oldLayout = ImageLayout::COLOR_ATTACHMENT;
            imageBarrier.newLayout = ImageLayout::PRESENT;
            barrier.imageBarriers.push_back(imageBarrier);
            commandBuffer->PipelineBarrier(barrier);
        }
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

        submit.fence = frameContext->GetFrameFence();
        device->GetQueue(QueueType::GRAPHICS)->Submit(submit);

        viewport->Release();
        if (hasPreview) {
            previewViewport->Release();
        }
        frameContext->EndFrame();
    }

    void EditorRenderer::Shutdown()
    {
        if (device != nullptr) {
            device->WaitIdle();
        }
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
