//
// Created on 2026/03/29.
//

#pragma once

#include <aurora/rhi/Fence.h>

#include <d3d12.h>
#include <wrl/client.h>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;

    class D3D12Fence : public Fence {
    public:
        explicit D3D12Fence(D3D12Device &dev);
        ~D3D12Fence() override;

        bool Init(const Descriptor &desc);

        void Wait() override;
        void Reset() override;

        ID3D12Fence *GetNativeHandle() const { return fence.Get(); }
        UINT64 GetPendingValue() const { return pendingValue; }

    private:
        D3D12Device      &device;
        ComPtr<ID3D12Fence> fence;
        HANDLE              event       = nullptr;
        UINT64              pendingValue = 0;
    };

} // namespace sky::aurora
