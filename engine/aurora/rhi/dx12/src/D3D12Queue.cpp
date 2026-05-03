//
// Aurora D3D12 Queue.
//

#include <D3D12Queue.h>
#include <D3D12Device.h>
#include <D3D12CommandPool.h>
#include <D3D12Fence.h>
#include <D3D12Semaphore.h>
#include <aurora/rhi/SubmitInfo.h>
#include <core/logger/Logger.h>
#include <vector>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {

    D3D12Queue::D3D12Queue(D3D12Device &dev, QueueType t, ComPtr<ID3D12CommandQueue> q)
        : device(dev)
        , type(t)
        , queue(std::move(q))
    {
    }

    D3D12Queue::~D3D12Queue()
    {
        if (idleEvent != nullptr) {
            ::CloseHandle(idleEvent);
            idleEvent = nullptr;
        }
    }

    bool D3D12Queue::Init()
    {
        HRESULT hr = device.GetNativeHandle()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&idleFence));
        if (FAILED(hr)) {
            LOG_E(TAG, "queue idle fence creation failed: 0x%08x", hr);
            return false;
        }
        idleEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
        return idleEvent != nullptr;
    }

    void D3D12Queue::Submit(const SubmitInfo &info)
    {
        // 1. Wait on input semaphores (queue-level Wait).
        for (const auto &w : info.waitSemaphores) {
            auto *sema = static_cast<D3D12Semaphore *>(w.semaphore);
            if (sema == nullptr) continue;
            const UINT64 value = (sema->GetType() == SemaphoreType::TIMELINE)
                                     ? w.value
                                     : sema->GetBinaryWaitValue();
            queue->Wait(sema->GetNativeHandle(), value);
        }

        // 2. Execute command lists.
        if (!info.commandBuffers.empty()) {
            std::vector<ID3D12CommandList*> lists;
            lists.reserve(info.commandBuffers.size());
            for (auto *cb : info.commandBuffers) {
                lists.push_back(static_cast<D3D12CommandBuffer *>(cb)->GetNativeHandle());
            }
            queue->ExecuteCommandLists(static_cast<UINT>(lists.size()), lists.data());
        }

        // 3. Signal output semaphores.
        for (const auto &s : info.signalSemaphores) {
            auto *sema = static_cast<D3D12Semaphore *>(s.semaphore);
            if (sema == nullptr) continue;
            const UINT64 value = (sema->GetType() == SemaphoreType::TIMELINE)
                                     ? s.value
                                     : sema->AdvanceBinarySignalValue();
            queue->Signal(sema->GetNativeHandle(), value);
        }

        // 4. Signal fence.
        if (info.fence != nullptr) {
            auto *fence = static_cast<D3D12Fence *>(info.fence);
            const UINT64 v = fence->BumpPendingValue();
            queue->Signal(fence->GetNativeHandle(), v);
        }
    }

    void D3D12Queue::WaitIdle()
    {
        ++idleValue;
        queue->Signal(idleFence.Get(), idleValue);
        if (idleFence->GetCompletedValue() < idleValue) {
            idleFence->SetEventOnCompletion(idleValue, idleEvent);
            ::WaitForSingleObject(idleEvent, INFINITE);
        }
    }

} // namespace sky::aurora
