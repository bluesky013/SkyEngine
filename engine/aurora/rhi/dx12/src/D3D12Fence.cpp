//
// Created on 2026/03/29.
//

#include <D3D12Fence.h>
#include <D3D12Device.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {

    D3D12Fence::D3D12Fence(D3D12Device &dev)
        : device(dev)
    {
    }

    D3D12Fence::~D3D12Fence()
    {
        if (event != nullptr) {
            ::CloseHandle(event);
        }
    }

    bool D3D12Fence::Init(const Descriptor &desc)
    {
        const UINT64 initialValue = desc.createSignaled ? 1 : 0;
        pendingValue = desc.createSignaled ? 0 : 1;

        HRESULT hr = device.GetNativeHandle()->CreateFence(
            initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        if (FAILED(hr)) {
            LOG_E(TAG, "failed to create ID3D12Fence, HRESULT: 0x%08x", hr);
            return false;
        }

        event = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (event == nullptr) {
            LOG_E(TAG, "failed to create Win32 event for fence");
            return false;
        }

        return true;
    }

    void D3D12Fence::Wait()
    {
        if (fence->GetCompletedValue() < pendingValue) {
            fence->SetEventOnCompletion(pendingValue, event);
            ::WaitForSingleObject(event, INFINITE);
        }
    }

    void D3D12Fence::Reset()
    {
        ++pendingValue;
    }

} // namespace sky::aurora
