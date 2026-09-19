//
// AuroraRender module: launcher integration for the Aurora RHI.
// Loaded as a dynamic module by framework ModuleManager (see AuroraRegistry.cpp).
//

#pragma once

#include <framework/interface/IModule.h>
#include <aurora/rdg/ClientViewport.h>
#include <aurora/rdg/RenderDeviceExclusive.h>
#include <aurora/rhi/CommandBuffer.h>
#include <aurora/rhi/Instance.h>
#include <aurora/rhi/Core.h>

#include <memory>

namespace sky::aurora {

    class Device;
    class DeviceFrameContext;
    class CommandPool;
    class CommandBuffer;
    class ClientViewport;

    class AuroraModule : public sky::IModule {
    public:
        AuroraModule() = default;
        ~AuroraModule() override;

        bool Init(const sky::StartArguments &args) override;
        void Start() override;
        void Tick(float delta) override;
        void Shutdown() override;

    private:
        void ProcessArgs(const sky::StartArguments &args);

        API         mApi    = API::DEFAULT;
        Device     *mDevice = nullptr;

        std::unique_ptr<DeviceFrameContext> mFrameContext;
        std::unique_ptr<CommandPool>        mCommandPool;
        CommandBuffer                      *mCommandBuffer = nullptr;
        std::unique_ptr<ClientViewport>     mViewport;
    };

} // namespace sky::aurora
