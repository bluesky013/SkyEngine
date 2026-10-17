#pragma once

#include <common/FrameContext.h>

namespace sky::aurora {
    class MetalDevice;

    // Serial Metal frame compiler for the offscreen subset (clear + copy +
    // readback). Draws, compute and custom passes remain explicit Unsupported.
    class MetalDeviceFrameContext final : public BackendFrameContext {
    public:
        explicit MetalDeviceFrameContext(MetalDevice *device) : device(device) {}

        FrameCompileResult  Compile(const DeviceFrameLease &, std::shared_ptr<const FrozenFrameGraph>) override;
        FrameDispatchResult Dispatch(std::unique_ptr<BackendCompiledFrame> &) override;

    private:
        FrameBufferStorageResult CreateFrameBuffer(const Buffer::Descriptor &) override;

        MetalDevice *device = nullptr;
    };

} // namespace sky::aurora
