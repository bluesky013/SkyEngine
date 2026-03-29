//
// Created on 2026/03/29.
//

#pragma once

#include <aurora/rhi/Semaphore.h>

#include <d3d12.h>
#include <wrl/client.h>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;

    class D3D12Semaphore : public Semaphore {
    public:
        explicit D3D12Semaphore(D3D12Device &dev);
        ~D3D12Semaphore() override;

        bool Init(const Descriptor &desc);

        uint64_t GetCurrentValue() const override;
        void Wait(uint64_t value) override;
        void Signal(uint64_t value) override;

        ID3D12Fence *GetNativeHandle() const { return fence.Get(); }

    private:
        D3D12Device        &device;
        ComPtr<ID3D12Fence> fence;
        HANDLE              event = nullptr;
    };

} // namespace sky::aurora
