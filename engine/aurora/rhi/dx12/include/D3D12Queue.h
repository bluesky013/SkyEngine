//
// Aurora D3D12 Queue.
//

#pragma once

#include <aurora/rhi/Queue.h>
#include <aurora/rhi/Buffer.h>
#include <aurora/rhi/Fence.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <memory>
#include <vector>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;
    class CommandPool;

    class D3D12Queue : public Queue {
    public:
        D3D12Queue(D3D12Device &dev, QueueType type, ComPtr<ID3D12CommandQueue> queue);
        ~D3D12Queue() override;

        bool Init();

        void      Submit(const SubmitInfo &info) override;
        void      WaitIdle() override;
        QueueType GetType() const override { return type; }

        TransferTaskHandle UploadBuffer(Buffer *buffer, const std::vector<BufferUploadRequest> &requests) override;
        TransferTaskHandle UploadImage(Image *image, const std::vector<ImageUploadRequest> &requests) override;

        void Wait(TransferTaskHandle handle) override;
        bool HasComplete(TransferTaskHandle handle) const override;

        ID3D12CommandQueue *GetNativeHandle() const { return queue.Get(); }

    private:
        struct PendingUpload {
            FencePtr                     fence;
            BufferPtr                    staging;
            std::unique_ptr<CommandPool> pool;
        };

        D3D12Device               &device;
        QueueType                  type;
        ComPtr<ID3D12CommandQueue> queue;
        ComPtr<ID3D12Fence>        idleFence;       // internal fence used by WaitIdle
        UINT64                     idleValue = 0;
        HANDLE                     idleEvent = nullptr;

        std::vector<PendingUpload> pendingUploads;
    };

} // namespace sky::aurora
