//
// Created on 2026/03/29.
//

#include <D3D12Semaphore.h>
#include <D3D12Device.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {

    D3D12Semaphore::D3D12Semaphore(D3D12Device &dev)
        : device(dev)
    {
    }

    D3D12Semaphore::~D3D12Semaphore()
    {
        if (event != nullptr) {
            ::CloseHandle(event);
        }
    }

    bool D3D12Semaphore::Init(const Descriptor &desc)
    {
        type = desc.type;
        const UINT64 initial = (type == SemaphoreType::TIMELINE) ? desc.initialValue : 0;

        HRESULT hr = device.GetNativeHandle()->CreateFence(
            initial, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&fence));
        if (FAILED(hr)) {
            LOG_E(TAG, "failed to create ID3D12Fence for semaphore, HRESULT: 0x%08x", hr);
            return false;
        }

        event = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (event == nullptr) {
            LOG_E(TAG, "failed to create Win32 event for semaphore");
            return false;
        }

        return true;
    }

    uint64_t D3D12Semaphore::GetCurrentValue() const
    {
        return fence->GetCompletedValue();
    }

    bool D3D12Semaphore::Wait(uint64_t value, uint64_t timeoutNs)
    {
        if (fence->GetCompletedValue() >= value) {
            return true;
        }
        fence->SetEventOnCompletion(value, event);
        const DWORD timeoutMs = static_cast<DWORD>(timeoutNs / 1'000'000ULL);
        return ::WaitForSingleObject(event, timeoutMs) == WAIT_OBJECT_0;
    }

    void D3D12Semaphore::Signal(uint64_t value)
    {
        fence->Signal(value);
    }

} // namespace sky::aurora
