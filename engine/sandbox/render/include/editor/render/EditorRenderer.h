//
// Created on 2026/09/21.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <framework/window/IWindowEvent.h>
#include <ui/UIPaintContext.h>
#if !defined(_WIN32)
    #include <ui/render/UIRenderer.h>
#endif
#include <aurora/adaptor/renderer/RenderSceneProxy.h>
#include <aurora/adaptor/renderer/RendererHost.h>
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
    class UiSnapshotProducer;

    class WindowEventBinding;
    class EditorFrameSubmission;

    class EditorRenderer : public sky::IWindowEvent {
    public:
        // Paint executes on the host thread; surface zero is the main window.
        using GuiPaintFn = std::function<void(uint32_t surfaceId, sky::ui::UIPaintContext &, uint32_t width, uint32_t height)>;

        EditorRenderer();
        ~EditorRenderer();

        EditorRenderer(const EditorRenderer &)            = delete;
        EditorRenderer &operator=(const EditorRenderer &) = delete;

        bool Init(const std::string         &appName,
                  uint32_t                   width,
                  uint32_t                   height,
                  sky::aurora::API           api         = sky::aurora::API::DEFAULT,
                  bool                       withPreview = true,
                  aurora::RendererVulkanMode mode        = aurora::RendererVulkanMode::DYNAMIC_RENDERING);
        void Start();
        void Tick(float delta);
        void Shutdown();

        // Native windows stay alive until their renderer retirement acknowledgement.
        void OnWindowClose(const sky::NativeWindow *window) override;
        void OnWindowMove(const sky::WindowMoveEvent &event) override;
        void OnWindowResize(const sky::WindowResizeEvent &event) override;

        bool IsInitialized() const
        {
#if defined(_WIN32)
            return host.Get() != nullptr;
#else
            return device != nullptr;
#endif
        }
#if defined(_WIN32)
        using SceneSource = std::function<void(aurora::FramePacket &, aurora::SceneId)>;
        void SetSceneSource(SceneSource source)
        {
            sceneSource = std::move(source);
        }
#endif

        ui::UITextureId GetSceneTextureId() const
        {
            return 1;
        }

        // GUI content source (the editor shell). Set after Init, before Start.
        void SetGuiSource(GuiPaintFn fn)
        {
            guiSource = std::move(fn);
        }
        // Text system (available after Init) so the shell can build text views.
        sky::ui::UITextSystem *GetTextSystem() const
        {
            return textSystem.get();
        }

        uint32_t CreateSurface(const std::string &panelId, int x, int y, uint32_t w, uint32_t h);
        // Queues retirement; a frame already being recorded retains the surface
        // through submit/present. Explicit destruction does not re-dock a panel.
        void     DestroySurface(uint32_t id);
        uint32_t SurfaceIdForWindow(const sky::NativeWindow *window) const;
        // DPI scale of a floating window (captured at creation); 1.0 if unknown.
        float SurfaceDpiScale(uint32_t id) const;
        void  SetSurfaceClosedCallback(std::function<void(const std::string &panelId)> callback)
        {
            surfaceClosed = std::move(callback);
        }
        // Reports a floating window's live geometry so it can be persisted.
        void SetSurfaceGeometryCallback(std::function<void(const std::string &panelId, float x, float y, float w, float h)> callback)
        {
            surfaceGeometryChanged = std::move(callback);
        }

    private:
#if defined(_WIN32)
        struct Surface {
            uint32_t                      id = 0;
            std::string                   panelId;
            std::unique_ptr<NativeWindow> owned;
            NativeWindow                 *window = nullptr;
            aurora::Extent2D              extent{}, submittedExtent{};
            uint64_t                      revision = 0;
            float                         dpiScale = 1;
            bool                          created = false, closed = false, retiring = false;
            ui::UIPaintContext            paint;
        };
        void                                        Poll();
        void                                        Fail(const aurora::RendererError &);
        bool                                        Submit();
        aurora::RendererHost                        host;
        aurora::RenderSceneProxy                    scene;
        std::unique_ptr<aurora::FramePacket>        pending;
        std::unique_ptr<UiSnapshotProducer>         textures;
        std::shared_ptr<const void>                 moduleLease;
        std::shared_ptr<aurora::IFrameGraphFeature> feature;
        std::vector<std::unique_ptr<Surface>>       surfaces;
        uint64_t                                    sequence      = 0;
        uint32_t                                    nextSurfaceId = 1;
        bool                                        started = false, withPreview = false;
        GuiPaintFn                                  guiSource;
        SceneSource                                 sceneSource;
        ui::UIBuiltinFontProvider                   builtinFont;
    #if defined(SKY_BUILD_FREETYPE)
        ui::FreeTypeUIFontProvider freeTypeFont;
    #endif
        std::unique_ptr<ui::UITextSystem>                                    textSystem;
        std::function<void(const std::string &)>                             surfaceClosed;
        std::function<void(const std::string &, float, float, float, float)> surfaceGeometryChanged;
#else

        void PaintUI(uint32_t surfaceId, uint32_t surfaceWidth, uint32_t surfaceHeight);
        void RenderFrame();

        // One floating window: its own swapchain, command buffer, UI renderer, and
        // paint context (so per-window GPU state stays isolated).
        struct FloatingSurface {
            uint32_t                                     id = 0;
            std::string                                  panelId;
            std::unique_ptr<sky::NativeWindow>           window;
            std::unique_ptr<WindowEventBinding>          events;
            std::unique_ptr<sky::aurora::ClientViewport> viewport;
            sky::aurora::CommandBuffer                  *commandBuffer = nullptr;
            std::unique_ptr<sky::ui::UIRenderer>         uiRenderer;
            sky::ui::UIPaintContext                      paintContext;
            float                                        x            = 0.0f;
            float                                        y            = 0.0f;
            uint32_t                                     w            = 0;
            uint32_t                                     h            = 0;
            float                                        dpiScale     = 1.0f;
            bool                                         closed       = false;
            bool                                         notifyClosed = false;
        };
        void RequestSurfaceClose(FloatingSurface &surface, bool notifyClosed);
        void RetireClosedSurfaces();
        void RenderSurface(FloatingSurface &surface, EditorFrameSubmission &frame);
        // Records the UI overlay pass (paint -> upload -> draw) and the barrier to
        // PRESENT for an already-cleared COLOR_ATTACHMENT backbuffer.
        void RecordUiPass(sky::aurora::CommandBuffer  *commandBuffer,
                          sky::aurora::Image          *backbuffer,
                          uint32_t                     width,
                          uint32_t                     height,
                          sky::ui::UIRenderer         &uiRenderer,
                          sky::ui::UIPaintContext     &paintContext,
                          const std::function<void()> &paint);

        GuiPaintFn guiSource;

        sky::aurora::Device                             *device = nullptr;
        std::unique_ptr<sky::aurora::DeviceFrameContext> frameContext;
        std::unique_ptr<sky::aurora::CommandPool>        commandPool;
        sky::aurora::CommandBuffer                      *commandBuffer = nullptr;
        std::unique_ptr<sky::aurora::ClientViewport>     viewport;
        sky::ui::UIRenderer                              uiRenderer;
        sky::ui::UIPaintContext                          paintContext;
        // Text: built-in glyph provider + atlas; glyph pages register through the
        // UIRenderer's IUITextureRegistry.
        sky::ui::UIBuiltinFontProvider builtinFont;
    #if defined(SKY_BUILD_FREETYPE)
        sky::ui::FreeTypeUIFontProvider freeTypeFont;
    #endif
        std::unique_ptr<sky::ui::UITextSystem> textSystem;
        // Viewport content target (TEXTURE presentation): an offscreen image the
        // scene renderer will draw into; composited as a UI image for now.
        sky::CounterPtr<sky::aurora::Image> contentTarget;
        // Standalone preview window (WINDOW presentation, scheme C: one submit
        // with a second command buffer + combined semaphores + shared fence).
        std::unique_ptr<sky::NativeWindow>                                   previewWindow;
        std::unique_ptr<WindowEventBinding>                                  previewEvents;
        std::unique_ptr<sky::aurora::ClientViewport>                         previewViewport;
        sky::CounterPtr<sky::aurora::Image>                                  previewTarget;
        sky::aurora::CommandBuffer                                          *previewCommandBuffer = nullptr;
        bool                                                                 previewClosed        = false;
        bool                                                                 frameActive          = false;
        bool                                                                 shutdownRequested    = false;
        std::vector<std::unique_ptr<FloatingSurface>>                        surfaces;
        uint32_t                                                             nextSurfaceId = 1;
        std::function<void(const std::string &)>                             surfaceClosed;
        std::function<void(const std::string &, float, float, float, float)> surfaceGeometryChanged;
        uint32_t                                                             width  = 1280;
        uint32_t                                                             height = 720;
#endif
    };

} // namespace sky::editor
