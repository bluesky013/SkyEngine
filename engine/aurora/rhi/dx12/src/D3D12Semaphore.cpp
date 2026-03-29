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
        HRESULT hr = device.GetNativeHandle()->CreateFence(
            desc.initialValue, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&fence));
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

    void D3D12Semaphore::Wait(uint64_t value)
    {
        if (fence->GetCompletedValue() < value) {
            fence->SetEventOnCompletion(value, event);
            ::WaitForSingleObject(event, INFINITE);
        }
    }

    void D3D12Semaphore::Signal(uint64_t value)
    {
        fence->Signal(value);
    }

} // namespace sky::aurora
