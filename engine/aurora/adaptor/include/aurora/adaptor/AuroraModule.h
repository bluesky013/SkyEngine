//
// Launcher packet producer; native command ownership stays with IRenderer.
//

#pragma once

#include <aurora/adaptor/renderer/RenderSceneProxy.h>
#include <aurora/adaptor/renderer/RendererHost.h>
#include <aurora/rhi/Core.h>
#include <framework/interface/IModule.h>
#include <framework/window/IWindowEvent.h>

#include <functional>
#include <memory>

namespace sky::aurora {

    class IRendererProvider;

    class AuroraModule : public sky::IModule, public sky::IWindowEvent {
    public:
        AuroraModule();
        AuroraModule(std::unique_ptr<IRendererProvider>, FileSystemPtr bundle);
        ~AuroraModule() override;

        bool Init(const sky::StartArguments &args) override;
        void Start() override;
        void Tick(float delta) override;
        void Shutdown() override;
        using SceneSource = std::function<void(FramePacket &, SceneId)>;
        void SetSceneSource(SceneSource source)
        {
            sceneSource = std::move(source);
        }
        void OnWindowResize(const WindowResizeEvent &) override;
        void OnSurfaceChanged(const NativeWindow *) override;
        void OnWindowClose(const NativeWindow *) override;

    private:
        void ProcessArgs(const sky::StartArguments &args);
        void PollEvents();
        void Pump(float delta, bool produceScene);
        void UpdateOutput(FramePacket &);
        void Fail(const RendererError &);

        SceneSource                  sceneSource;
        RendererHost                 host;
        RenderSceneProxy             scene;
        std::unique_ptr<FramePacket> pending;
        NativeWindow                *window = nullptr;
        FileSystemPtr                bundle;
        std::shared_ptr<const void>  desiredSurfaceLease;
        std::shared_ptr<const void>  activeSurfaceLease;
        uintptr_t                    desiredSurface = 0;
        uintptr_t                    activeSurface  = 0;
        ViewportId                   viewport{0, 1};
        ViewId                       view{0, 1};
        ViewDesc                     viewDescription;
        enum class OutputState : uint8_t { ABSENT, LIVE, RETIRING };
        OutputState outputState = OutputState::ABSENT;
        Extent2D    extent{1280, 720};
        Extent2D    submittedExtent{1280, 720};
        uint64_t    sequence      = 0;
        uint64_t    revision      = 0;
        uint64_t    viewRevision  = 0;
        bool        viewCreated   = false;
        bool        replaceOutput = false;
        bool        started       = false;
        bool        closed        = false;
        bool        retiring      = false;
        bool        pumping       = false;

        RendererVulkanMode vulkanMode = RendererVulkanMode::DYNAMIC_RENDERING;

        API  mApi         = API::DEFAULT;
        bool providerOnly = false;

        std::unique_ptr<IRendererProvider> provider;
    };

} // namespace sky::aurora
