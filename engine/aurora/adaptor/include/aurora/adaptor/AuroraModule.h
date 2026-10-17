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

    class Device;
    class DeviceFrameContext;
    class CommandPool;
    class CommandBuffer;
    class ClientViewport;
    class IRendererProvider;

    class AuroraModule : public sky::IModule, public sky::IWindowEvent {
    public:
        AuroraModule();
        ~AuroraModule() override;

        void RegisterMeta() override;
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
        void OnWindowClose(const NativeWindow *) override;

    private:
        void ProcessArgs(const sky::StartArguments &args);
#if !defined(_WIN32)
        bool InitLegacy();
        void StartLegacy();
        void TickLegacy();
#endif
        void PollEvents();
        void Fail(const RendererError &);

        SceneSource                  sceneSource;
        RendererHost                 host;
        RenderSceneProxy             scene;
        std::unique_ptr<FramePacket> pending;
        NativeWindow                *window = nullptr;
        Extent2D                     extent{1280, 720};
        Extent2D                     submittedExtent{1280, 720};
        uint64_t                     sequence   = 0;
        uint64_t                     revision   = 0;
        bool                         started    = false;
        bool                         closed     = false;
        bool                         retiring   = false;

        RendererVulkanMode vulkanMode = RendererVulkanMode::DYNAMIC_RENDERING;

        API     mApi         = API::DEFAULT;
        bool    providerOnly = false;
#if !defined(_WIN32)
        bool    legacy  = false;
        Device *mDevice = nullptr;

        std::unique_ptr<DeviceFrameContext> mFrameContext;
        std::unique_ptr<CommandPool>        mCommandPool;
        CommandBuffer                      *mCommandBuffer = nullptr;
        std::unique_ptr<ClientViewport>     mViewport;
#endif
        std::unique_ptr<IRendererProvider> provider;
    };

} // namespace sky::aurora
