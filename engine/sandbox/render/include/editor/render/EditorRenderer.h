//
// Created on 2026/09/21.
//

#pragma once

#include <aurora/rhi/Instance.h>
#include <ui/UIPaintContext.h>
#include <ui/render/UIRenderer.h>
#include <ui/text/UIBuiltinFontProvider.h>
#include <ui/text/UITextSystem.h>
#if defined(SKY_BUILD_FREETYPE)
#include <ui/freetype/FreeTypeUIFontProvider.h>
#endif
#include <core/template/ReferenceObject.h>
#include <cstdint>
#include <memory>
#include <string>

namespace sky {
    class NativeWindow;
} // namespace sky

namespace sky::aurora {
    class ClientViewport;
    class CommandBuffer;
    class CommandPool;
    class Device;
    class DeviceFrameContext;
    class Image;
} // namespace sky::aurora

namespace sky::editor {

    // Editor-owned renderer.
    //
    // Owns the editor frame on the Aurora device: device + frame context +
    // command pool + the main-window `ClientViewport`, running the hardcoded
    // frame (barrier -> scene pass -> UI pass -> present). The GUI pipeline
    // itself lives in `sky::ui::UIRenderer`; this class only hosts the frame and
    // feeds it draw data. `SandboxModule` delegates to it.
    class EditorRenderer {
    public:
        EditorRenderer();
        ~EditorRenderer();

        EditorRenderer(const EditorRenderer &) = delete;
        EditorRenderer &operator=(const EditorRenderer &) = delete;

        bool Init(const std::string &appName, uint32_t width, uint32_t height,
                  sky::aurora::API api = sky::aurora::API::DEFAULT);
        // Creates the viewport from the main window handle (via ISystemNotify).
        void Start();
        void Tick(float delta);
        void Shutdown();

        bool IsInitialized() const { return device != nullptr; }

    private:
        void PaintUI(uint32_t surfaceWidth, uint32_t surfaceHeight);

        sky::aurora::Device                             *device = nullptr;
        std::unique_ptr<sky::aurora::DeviceFrameContext> frameContext;
        std::unique_ptr<sky::aurora::CommandPool>        commandPool;
        sky::aurora::CommandBuffer                      *commandBuffer = nullptr;
        std::unique_ptr<sky::aurora::ClientViewport>     viewport;
        sky::ui::UIRenderer                              uiRenderer;
        sky::ui::UIPaintContext                          paintContext;
        // Text: built-in glyph provider + atlas; glyph pages register through the
        // UIRenderer's IUITextureRegistry.
        sky::ui::UIBuiltinFontProvider                   builtinFont;
#if defined(SKY_BUILD_FREETYPE)
        sky::ui::FreeTypeUIFontProvider                  freeTypeFont;
#endif
        std::unique_ptr<sky::ui::UITextSystem>           textSystem;
        // Viewport content target (TEXTURE presentation): an offscreen image the
        // scene renderer will draw into; composited as a UI image for now.
        sky::CounterPtr<sky::aurora::Image>              contentTarget;
        // Standalone preview window (WINDOW presentation, scheme C: one submit
        // with a second command buffer + combined semaphores + shared fence).
        std::unique_ptr<sky::NativeWindow>               previewWindow;
        std::unique_ptr<sky::aurora::ClientViewport>     previewViewport;
        sky::CounterPtr<sky::aurora::Image>              previewTarget;
        sky::aurora::CommandBuffer                      *previewCommandBuffer = nullptr;
        uint32_t                                         width  = 1280;
        uint32_t                                         height = 720;
    };

} // namespace sky::editor
