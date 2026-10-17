#pragma once

#include <common/FrameContext.h>
#include <core/platform/Platform.h>
#include <d3d12.h>
#include <wrl/client.h>

#ifdef Aurora_DX12_EXPORTS
    #define AURORA_DX12_FRAME_API SKY_EXPORT
#else
    #define AURORA_DX12_FRAME_API SKY_IMPORT
#endif

namespace sky::aurora {
    class D3D12Device;
    struct D3D12FrameDescriptors {
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
        D3D12_CPU_DESCRIPTOR_HANDLE                  cpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE                  gpu{};
    };
    using D3D12DescriptorAllocation = std::variant<FrameError, D3D12FrameDescriptors>;

    class D3D12DeviceFrameContext final : public BackendFrameContext {
    public:
        explicit D3D12DeviceFrameContext(D3D12Device *device);
        ~D3D12DeviceFrameContext() override;
        FrameCompileResult   Compile(const DeviceFrameLease &, std::shared_ptr<const FrozenFrameGraph>) override;
        FrameDispatchResult  Dispatch(std::unique_ptr<BackendCompiledFrame> &) override;
        FrameCollectResult   TryCollect(const EncodingTicket &) override;
        FrameOperationResult Cancel(const EncodingTicket &) override;

        AURORA_DX12_FRAME_API D3D12DescriptorAllocation AllocateDescriptors(const DeviceFrameLease &, D3D12_DESCRIPTOR_HEAP_TYPE, uint32_t count);
        FrameBufferAllocation                           AllocateCopyScratch(const DeviceFrameLease &lease, uint64_t size);

    protected:
        void SelectDirectSlot(uint32_t slot) noexcept override;

    private:
        FrameOperationResult     ResetLeaseResources(uint32_t) noexcept override;
        FrameBufferStorageResult CreateFrameBuffer(const Buffer::Descriptor &) override;
        bool                     InitBackend(const DeviceFrameContextInitInfo &) override;
        struct State;
        struct EncodingSession;
        std::unique_ptr<State> state;
        D3D12Device           *device;
        FrameRecordResult      Finish(EncodingSession &);
    };

} // namespace sky::aurora
