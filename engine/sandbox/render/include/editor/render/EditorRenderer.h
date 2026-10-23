//
// Created on 2026/09/21.
//

#pragma once

#include <aurora/adaptor/renderer/RenderSceneProxy.h>
#include <aurora/adaptor/renderer/RendererHost.h>
#include <aurora/rhi/Core.h>
#include <editor/core/viewport/EditorCamera.h>
#include <framework/window/IWindowEvent.h>
#include <optional>
#include <ui/UIPaintContext.h>
#include <ui/text/UIBuiltinFontProvider.h>
#include <ui/text/UITextSystem.h>
#if defined(SKY_BUILD_FREETYPE)
    #include <ui/freetype/FreeTypeUIFontProvider.h>
#endif
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace sky {
    class NativeWindow;
    class World;
} // namespace sky

namespace sky::aurora {
    class RenderSceneBridge;
} // namespace sky::aurora

namespace sky::editor {
    class UiSnapshotProducer;

    class EditorFramePacer;
    class EditorViewportState;
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
        void OnFocusChanged(bool focus) override
        {
            if (!focus && inputLost)
                inputLost();
        }
        void SetInputLostCallback(std::function<void()> callback)
        {
            inputLost = std::move(callback);
        }
        void SetEditorCamera(std::optional<EditorCamera> camera)
        {
            editorCamera = std::move(camera);
        }

        bool IsInitialized() const
        {
            return host.Get() != nullptr;
        }
        using SceneSource = std::function<void(aurora::FramePacket &, aurora::SceneId)>;
        void SetWorld(World *world);
        void SetSceneExtent(aurora::Extent2D extent);
        void SetSceneSource(SceneSource source)
        {
            sceneSource = std::move(source);
        }

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
        std::optional<EditorCamera> editorCamera;
        std::function<void()>       inputLost;
        struct Surface {
            explicit Surface(uint32_t id);
            ~Surface();
            uint32_t                             id = 0;
            std::string                          panelId;
            std::unique_ptr<NativeWindow>        owned;
            NativeWindow                        *window = nullptr;
            aurora::Extent2D                     extent{}, submittedExtent{};
            uint64_t                             revision = 0;
            float                                dpiScale = 1;
            bool                                 closed = false, closeNotified = false;
            std::unique_ptr<EditorViewportState> output;
            ui::UIPaintContext                   paint;
        };
        void                                        Poll();
        void                                        Fail(const aurora::RendererError &);
        bool                                        Submit();
        aurora::RendererHost                        host;
        aurora::RenderSceneProxy                    scene;
        std::unique_ptr<aurora::FramePacket>        pending;
        std::unique_ptr<EditorFramePacer>           framePacer;
        std::unique_ptr<UiSnapshotProducer>         textures;
        std::shared_ptr<const void>                 moduleLease;
        std::shared_ptr<aurora::IFrameGraphFeature> feature;
        std::vector<std::unique_ptr<Surface>>       surfaces;
        uint64_t                                    sequence      = 0;
        uint32_t                                    nextSurfaceId = 1;
        bool                                        started = false, withPreview = false;
        GuiPaintFn                                  guiSource;
        SceneSource                                 sceneSource;
        std::unique_ptr<aurora::RenderSceneBridge>  sceneBridge;
        std::shared_ptr<aurora::ShaderFileSystem>   shaderFiles;
        aurora::Extent2D                            sceneExtent{640, 360}, submittedSceneExtent{640, 360};
        uint64_t                                    sceneRevision = 0, viewRevision = 0;
        std::string                                 sceneIssue;
        ui::UIBuiltinFontProvider                   builtinFont;
#if defined(SKY_BUILD_FREETYPE)
        ui::FreeTypeUIFontProvider freeTypeFont;
#endif
        std::unique_ptr<ui::UITextSystem>                                    textSystem;
        std::function<void(const std::string &)>                             surfaceClosed;
        std::function<void(const std::string &, float, float, float, float)> surfaceGeometryChanged;
    };

} // namespace sky::editor
