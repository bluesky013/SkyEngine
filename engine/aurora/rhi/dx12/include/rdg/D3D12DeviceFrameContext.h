//
// Created on 2026/08/31.
//

#pragma once

#include <D3D12CommandPool.h>
#include <aurora/rdg/RenderDeviceExclusive.h>

namespace sky::aurora {
    class D3D12Device;

    class D3D12DeviceFrameContext : public DeviceFrameContext {
    public:
        explicit D3D12DeviceFrameContext(D3D12Device *device, const DeviceFrameContextInitInfo &info);
        ~D3D12DeviceFrameContext() noexcept override;

    private:
        D3D12Device                 *mDevice;
        std::unique_ptr<CommandPool> mPool;
        D3D12CommandPool            *mD3D12Pool = nullptr;
        std::vector<CommandBuffer *> mBuffers;

        std::vector<std::vector<CommandBuffer *>> mParallelBuffers;
    };

} // namespace sky::aurora
