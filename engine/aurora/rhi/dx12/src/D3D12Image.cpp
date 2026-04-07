//
// Created on 2026/04/02.
//

#include "D3D12Image.h"
#include "D3D12Device.h"
#include "D3D12Conversion.h"
#include <core/logger/Logger.h>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {

    D3D12Image::D3D12Image(D3D12Device &dev)
        : device(dev)
    {
    }

    D3D12Image::~D3D12Image()
    {
        resource.Reset();
        allocation.Reset();
    }

    bool D3D12Image::Init(const Descriptor &desc)
    {
        dxgiFormat = FromPixelFormat(desc.format);
        if (dxgiFormat == DXGI_FORMAT_UNKNOWN) {
            LOG_E(TAG, "unsupported pixel format for image");
            return false;
        }

        D3D12MA::ALLOCATION_DESC allocDesc = {};
        allocDesc.HeapType = FromMemoryType(desc.memory);

        D3D12_RESOURCE_DESC resDesc = {};
        resDesc.Dimension        = FromImageType(desc.imageType);
        resDesc.Alignment        = 0;
        resDesc.Width            = desc.extent.width;
        resDesc.Height           = desc.extent.height;
        resDesc.DepthOrArraySize = static_cast<UINT16>(
            desc.imageType == ImageType::IMAGE_3D ? desc.extent.depth : desc.arrayLayers);
        resDesc.MipLevels        = static_cast<UINT16>(desc.mipLevels);
        resDesc.Format           = dxgiFormat;
        resDesc.SampleDesc       = {static_cast<UINT>(desc.samples), 0};
        resDesc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resDesc.Flags            = FromImageUsageFlags(desc.usage);

        // Choose optimal clear value for render targets / depth-stencil
        D3D12_CLEAR_VALUE clearValue = {};
        const D3D12_CLEAR_VALUE *pClearValue = nullptr;

        if (desc.usage & ImageUsageFlagBit::RENDER_TARGET) {
            clearValue.Format = dxgiFormat;
            clearValue.Color[0] = 0.f;
            clearValue.Color[1] = 0.f;
            clearValue.Color[2] = 0.f;
            clearValue.Color[3] = 1.f;
            pClearValue = &clearValue;
        } else if (desc.usage & ImageUsageFlagBit::DEPTH_STENCIL) {
            clearValue.Format = dxgiFormat;
            clearValue.DepthStencil.Depth   = 1.f;
            clearValue.DepthStencil.Stencil = 0;
            pClearValue = &clearValue;
        }

        D3D12MA::Allocation *pAlloc = nullptr;
        const HRESULT hr = device.GetAllocator()->CreateResource(
            &allocDesc,
            &resDesc,
            D3D12_RESOURCE_STATE_COMMON,
            pClearValue,
            &pAlloc,
            IID_PPV_ARGS(resource.GetAddressOf()));

        if (FAILED(hr)) {
            LOG_E(TAG, "D3D12MA CreateResource (image) failed: 0x%08x", hr);
            return false;
        }
        allocation.Attach(pAlloc);

        return true;
    }

    void D3D12Image::InitFromSwapChain(ComPtr<ID3D12Resource> res, DXGI_FORMAT fmt)
    {
        resource   = std::move(res);
        dxgiFormat = fmt;
    }

} // namespace sky::aurora
