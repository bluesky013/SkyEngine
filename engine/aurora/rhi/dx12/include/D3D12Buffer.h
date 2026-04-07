//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/Buffer.h>

#include <d3d12.h>
#include <D3D12MemAlloc.h>
#include <wrl/client.h>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;

    class D3D12Buffer : public Buffer {
    public:
        explicit D3D12Buffer(D3D12Device &dev);
        ~D3D12Buffer() override;

        bool Init(const Descriptor &desc);

        ID3D12Resource *GetNativeHandle() const { return resource.Get(); }

        uint8_t *Map();
        void UnMap();

    private:
        D3D12Device            &device;
        ComPtr<ID3D12Resource>  resource;
        ComPtr<D3D12MA::Allocation> allocation;
        uint64_t                size = 0;
    };

} // namespace sky::aurora
