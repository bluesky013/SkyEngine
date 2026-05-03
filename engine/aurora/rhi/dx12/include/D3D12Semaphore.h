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

        SemaphoreType GetType() const override { return type; }
        void          Signal(uint64_t value) override;
        bool          Wait(uint64_t value, uint64_t timeoutNs) override;
        uint64_t      GetCurrentValue() const override;

        ID3D12Fence *GetNativeHandle() const { return fence.Get(); }

        // Backend-only: each Submit-signal of a binary semaphore advances by 1;
        // the matching Submit-wait reads it.
        uint64_t AdvanceBinarySignalValue() { return ++binaryValue; }
        uint64_t GetBinaryWaitValue() const { return binaryValue; }

    private:
        D3D12Device        &device;
        ComPtr<ID3D12Fence> fence;
        HANDLE              event = nullptr;
        SemaphoreType       type  = SemaphoreType::BINARY;
        UINT64              binaryValue = 0;
    };

} // namespace sky::aurora
