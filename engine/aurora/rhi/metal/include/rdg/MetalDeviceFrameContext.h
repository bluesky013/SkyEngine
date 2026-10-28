#pragma once

#include <common/FrameContext.h>

namespace sky::aurora {
    class MetalDevice;

    class MetalDeviceFrameContext final : public BackendFrameContext {
    public:
        explicit MetalDeviceFrameContext(MetalDevice *device) : device(device) {}

        FrameCompileResult  Compile(const DeviceFrameLease &, std::shared_ptr<const FrozenFrameGraph>) override;
        FrameDispatchResult Dispatch(std::unique_ptr<BackendCompiledFrame> &) override;

    private:
        FrameBufferStorageResult CreateFrameBuffer(const Buffer::Descriptor &) override;

        MetalDevice *device = nullptr;
        const BackendCompiledFrame *issuedPlan = nullptr;
        CompiledFrameIdentity       issuedIdentity;
    };

} // namespace sky::aurora
