//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/Image.h>

#include <d3d12.h>
#include <wrl/client.h>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;

    class D3D12Image : public Image {
    public:
        explicit D3D12Image(D3D12Device &dev);
        ~D3D12Image() override = default;

        bool Init(const Descriptor &desc);

        // Adopt an externally-owned resource (e.g. swapchain back buffer).
        void InitFromSwapChain(ComPtr<ID3D12Resource> res, DXGI_FORMAT fmt);

        ID3D12Resource *GetNativeHandle() const { return resource.Get(); }
        DXGI_FORMAT     GetDxgiFormat() const { return dxgiFormat; }

    private:
        D3D12Device            &device;
        ComPtr<ID3D12Resource>  resource;
        DXGI_FORMAT             dxgiFormat = DXGI_FORMAT_UNKNOWN;
    };

} // namespace sky::aurora
