//
// Created on 2026/03/29.
//

#include <D3D12Device.h>
#include <D3D12Fence.h>
#include <algorithm>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {
    const Device *D3D12Fence::GetDevice() const noexcept
    {
        return &device;
    }

    D3D12Fence::D3D12Fence(D3D12Device &dev) : device(dev)
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
        pendingValue              = 1;

        HRESULT hr = device.GetNativeHandle()->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
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
        const auto result = TryReset();
        if (!result.Succeeded())
            LOG_E(TAG, "Fence reset failed: %lld", result.error.nativeCode);
    }

    bool D3D12Fence::IsSignaled()
    {
        return Query().status == FenceStatus::SIGNALED;
    }

    GpuOperationResult D3D12Fence::TryReset() noexcept
    {
        const auto state = Query();
        if (state.status == FenceStatus::FAILED)
            return {state.error};
        if (pendingValue >= UINT64_MAX - 1)
            return {{GpuErrorCode::INVALID_ARGUMENT}};
        ++pendingValue;
        return {};
    }

    FenceQueryResult D3D12Fence::Query() noexcept
    {
        const auto completed = device.GetFunctions().completed(fence.Get());
        if (completed == UINT64_MAX)
            return {FenceStatus::FAILED, {GpuErrorCode::DEVICE_LOST, device.GetFunctions().removed(device.GetNativeHandle())}};
        return {completed >= pendingValue ? FenceStatus::SIGNALED : FenceStatus::PENDING};
    }

    bool D3D12Fence::WaitFor(uint64_t timeoutNs)
    {
        const auto state = Query();
        if (state.status == FenceStatus::FAILED)
            return false;
        if (state.status == FenceStatus::SIGNALED) {
            return true;
        }
        if (FAILED(fence->SetEventOnCompletion(pendingValue, event)))
            return false;
        const DWORD timeoutMs = static_cast<DWORD>((std::min)(timeoutNs / 1'000'000ULL, uint64_t{INFINITE - 1}));
        return ::WaitForSingleObject(event, timeoutMs) == WAIT_OBJECT_0 && Query().status == FenceStatus::SIGNALED;
    }

} // namespace sky::aurora
