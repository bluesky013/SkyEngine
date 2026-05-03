//
// Aurora D3D12 Queue.
//

#pragma once

#include <aurora/rhi/Queue.h>
#include <d3d12.h>
#include <wrl/client.h>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;

    class D3D12Queue : public Queue {
    public:
        D3D12Queue(D3D12Device &dev, QueueType type, ComPtr<ID3D12CommandQueue> queue);
        ~D3D12Queue() override;

        bool Init();

        void      Submit(const SubmitInfo &info) override;
        void      WaitIdle() override;
        QueueType GetType() const override { return type; }

        ID3D12CommandQueue *GetNativeHandle() const { return queue.Get(); }

    private:
        D3D12Device              &device;
        QueueType                 type;
        ComPtr<ID3D12CommandQueue> queue;
        ComPtr<ID3D12Fence>        idleFence;       // internal fence used by WaitIdle
        UINT64                     idleValue = 0;
        HANDLE                     idleEvent = nullptr;
    };

} // namespace sky::aurora
