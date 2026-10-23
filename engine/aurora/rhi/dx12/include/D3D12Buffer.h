//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rdg/FrameExecutionTypes.h>
#include <aurora/rhi/Buffer.h>
#include <aurora/rhi/ShaderReflection.h>

#include <D3D12MemAlloc.h>
#include <d3d12.h>
#include <optional>
#include <wrl/client.h>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;

    class D3D12Buffer : public Buffer {
    public:
        const Device *GetDevice() const noexcept override;
        explicit D3D12Buffer(D3D12Device &dev);
        ~D3D12Buffer() override;

        bool    Init(const Descriptor &desc);
        HRESULT GetCreateResult() const noexcept
        {
            return createResult;
        }

        ID3D12Resource *GetNativeHandle() const
        {
            return resource.Get();
        }

        HRESULT GetMapResult() const noexcept
        {
            return mapResult;
        }
        uint8_t *Map() override;
        void     UnMap() override;

        uint64_t GetSize() const override
        {
            return size;
        }
        const Descriptor &GetDescriptor() const noexcept override
        {
            return descriptor;
        }

        // Native storage views preserve the reflected access, element stride and exact byte range.
        void                      CreateCBV(D3D12_CPU_DESCRIPTOR_HANDLE handle, uint64_t offset, uint64_t range) const;
        std::optional<FrameError> CheckStorageRange(const ShaderResource &, uint64_t offset, uint64_t range) const;
        std::optional<FrameError> CreateStorageView(D3D12_CPU_DESCRIPTOR_HANDLE, const ShaderResource &, uint64_t offset, uint64_t range) const;

    private:
        HRESULT                     mapResult    = S_OK;
        HRESULT                     createResult = S_OK;
        D3D12Device                &device;
        Descriptor                  descriptor;
        ComPtr<ID3D12Resource>      resource;
        ComPtr<D3D12MA::Allocation> allocation;
        uint64_t                    size = 0;
    };

} // namespace sky::aurora
