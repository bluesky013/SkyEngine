//
// Created on 2026/09/21.
//

#pragma once

#include <aurora/rhi/Instance.h>
#include <framework/window/IWindowEvent.h>
#include <ui/UIPaintContext.h>
#include <ui/render/UIRenderer.h>
#include <ui/text/UIBuiltinFontProvider.h>
#include <ui/text/UITextSystem.h>
#if defined(SKY_BUILD_FREETYPE)
#include <ui/freetype/FreeTypeUIFontProvider.h>
#endif
#include <core/template/ReferenceObject.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

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
    class EditorRenderer : public sky::IWindowEvent {
    public:
        // Fills the GUI paint context for a surface. `surfaceId` identifies the
        // target window (0 = main; floating surfaces get their own id) so the
        // shell can paint the matching UI context. The renderer never knows the
        // shell type.
        using GuiPaintFn =
            std::function<void(uint32_t surfaceId, sky::ui::UIPaintContext &, uint32_t width, uint32_t height)>;

        EditorRenderer();
        ~EditorRenderer();

        EditorRenderer(const EditorRenderer &) = delete;
        EditorRenderer &operator=(const EditorRenderer &) = delete;

        bool Init(const std::string &appName, uint32_t width, uint32_t height,
                  sky::aurora::API api = sky::aurora::API::DEFAULT, bool withPreview = true);
        // Creates the viewport from the main window handle (via ISystemNotify).
        void Start();
        void Tick(float delta);
        void Shutdown();

        // Stops presenting to the standalone preview window once it is closed
        // (event-driven; the viewport/window are dropped on the next Tick).
        void OnWindowClose(const sky::NativeWindow *window) override;
        void OnWindowMove(const sky::WindowMoveEvent &event) override;
        void OnWindowResize(const sky::WindowResizeEvent &event) override;

        bool IsInitialized() const { return device != nullptr; }

        // GUI content source (the editor shell). Set after Init, before Start.
        void SetGuiSource(GuiPaintFn fn) { guiSource = std::move(fn); }
        // Text system (available after Init) so the shell can build text views.
        sky::ui::UITextSystem *GetTextSystem() const { return textSystem.get(); }

        // Floating windows for teared-out panels (Win32 v1). Each surface owns its
        // own swapchain + UI renderer and paints the shell's matching context.
        uint32_t CreateSurface(const std::string &panelId, int x, int y, uint32_t w, uint32_t h);
        void DestroySurface(uint32_t id);
        uint32_t SurfaceIdForWindow(const sky::NativeWindow *window) const;
        // DPI scale of a floating window (captured at creation); 1.0 if unknown.
        float SurfaceDpiScale(uint32_t id) const;
        void SetSurfaceClosedCallback(std::function<void(const std::string &panelId)> callback)
        {
            surfaceClosed = std::move(callback);
        }
        // Reports a floating window's live geometry so it can be persisted.
        void SetSurfaceGeometryCallback(
            std::function<void(const std::string &panelId, float x, float y, float w, float h)> callback)
        {
            surfaceGeometryChanged = std::move(callback);
        }

    private:
        void PaintUI(uint32_t surfaceId, uint32_t surfaceWidth, uint32_t surfaceHeight);

        // One floating window: its own swapchain, command buffer, UI renderer, and
        // paint context (so per-window GPU state stays isolated).
        struct FloatingSurface {
            uint32_t                                     id = 0;
            std::string                                  panelId;
            std::unique_ptr<sky::NativeWindow>           window;
            std::unique_ptr<sky::aurora::ClientViewport> viewport;
            sky::aurora::CommandBuffer                  *commandBuffer = nullptr;
            std::unique_ptr<sky::ui::UIRenderer>         uiRenderer;
            sky::ui::UIPaintContext                      paintContext;
            float                                        x = 0.0f;
            float                                        y = 0.0f;
            uint32_t                                     w = 0;
            uint32_t                                     h = 0;
            float                                        dpiScale = 1.0f;
            bool                                         closed = false;
        };
        bool RenderSurface(FloatingSurface &surface);
        // Records the UI overlay pass (paint -> upload -> draw) and the barrier to
        // PRESENT for an already-cleared COLOR_ATTACHMENT backbuffer.
        void RecordUiPass(sky::aurora::CommandBuffer *commandBuffer, sky::aurora::Image *backbuffer, uint32_t width,
                          uint32_t height, sky::ui::UIRenderer &uiRenderer, sky::ui::UIPaintContext &paintContext,
                          const std::function<void()> &paint);

        GuiPaintFn                                       guiSource;

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
        bool                                             previewClosed = false;
        std::vector<std::unique_ptr<FloatingSurface>>    surfaces;
        uint32_t                                         nextSurfaceId = 1;
        std::function<void(const std::string &)>         surfaceClosed;
        std::function<void(const std::string &, float, float, float, float)> surfaceGeometryChanged;
        uint32_t                                         width  = 1280;
        uint32_t                                         height = 720;
    };

} // namespace sky::editor
