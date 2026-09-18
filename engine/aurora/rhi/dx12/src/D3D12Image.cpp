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
        pixelFormat = desc.format;
        mipLevels   = desc.mipLevels;
        imageType   = desc.imageType;
        arrayLayers = desc.arrayLayers;
        depth       = desc.extent.depth;
        samples     = desc.samples;
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

#if SKY_ENABLE_RESOURCE_NAME
        if (desc.name != nullptr && resource) {
            std::wstring wideName;
            for (const char *c = desc.name; *c != '\0'; ++c) {
                wideName.push_back(static_cast<wchar_t>(*c));
            }
            resource->SetName(wideName.c_str());
        }
#endif

        return true;
    }

    void D3D12Image::InitFromSwapChain(ComPtr<ID3D12Resource> res, DXGI_FORMAT fmt)
    {
        resource   = std::move(res);
        dxgiFormat = fmt;
    }

    void D3D12Image::CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE handle) const
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv   = {};
        srv.Format                           = dxgiFormat;
        srv.Shader4ComponentMapping          = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        if (imageType == ImageType::IMAGE_1D) {
            if (arrayLayers > 1) {
                srv.ViewDimension              = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
                srv.Texture1DArray.MipLevels   = mipLevels;
                srv.Texture1DArray.ArraySize   = arrayLayers;
            } else {
                srv.ViewDimension            = D3D12_SRV_DIMENSION_TEXTURE1D;
                srv.Texture1D.MipLevels      = mipLevels;
            }
        } else if (imageType == ImageType::IMAGE_3D) {
            srv.ViewDimension        = D3D12_SRV_DIMENSION_TEXTURE3D;
            srv.Texture3D.MipLevels  = mipLevels;
        } else if (samples != SampleCount::X1) {
            if (arrayLayers > 1) {
                srv.ViewDimension               = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
                srv.Texture2DMSArray.ArraySize  = arrayLayers;
            } else {
                srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
            }
        } else if (arrayLayers > 1) {
            srv.ViewDimension            = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srv.Texture2DArray.MipLevels = mipLevels;
            srv.Texture2DArray.ArraySize = arrayLayers;
        } else {
            srv.ViewDimension       = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv.Texture2D.MipLevels = mipLevels;
        }

        device.GetNativeHandle()->CreateShaderResourceView(resource.Get(), &srv, handle);
    }

    void D3D12Image::CreateUAV(D3D12_CPU_DESCRIPTOR_HANDLE handle) const
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
        uav.Format                          = dxgiFormat;

        if (imageType == ImageType::IMAGE_1D) {
            uav.ViewDimension       = D3D12_UAV_DIMENSION_TEXTURE1D;
            uav.Texture1D.MipSlice  = 0;
        } else if (imageType == ImageType::IMAGE_3D) {
            uav.ViewDimension       = D3D12_UAV_DIMENSION_TEXTURE3D;
            uav.Texture3D.MipSlice  = 0;
            uav.Texture3D.WSize     = depth;
        } else {
            uav.ViewDimension       = D3D12_UAV_DIMENSION_TEXTURE2D;
            uav.Texture2D.MipSlice  = 0;
        }

        device.GetNativeHandle()->CreateUnorderedAccessView(resource.Get(), nullptr, &uav, handle);
    }

    void D3D12Image::CreateRTV(D3D12_CPU_DESCRIPTOR_HANDLE handle, const ImageSubRange &range) const
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtv = {};
        rtv.Format                        = dxgiFormat;

        const uint32_t firstLayer = range.baseLayer;
        const uint32_t arraySize  = range.layers;
        if (imageType == ImageType::IMAGE_3D) {
            rtv.ViewDimension           = D3D12_RTV_DIMENSION_TEXTURE3D;
            rtv.Texture3D.MipSlice      = range.baseLevel;
            rtv.Texture3D.FirstWSlice   = firstLayer;
            rtv.Texture3D.WSize         = arraySize;
        } else if (samples != SampleCount::X1) {
            if (arrayLayers > 1) {
                rtv.ViewDimension                     = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                rtv.Texture2DMSArray.FirstArraySlice  = firstLayer;
                rtv.Texture2DMSArray.ArraySize        = arraySize;
            } else {
                rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            }
        } else if (arrayLayers > 1) {
            rtv.ViewDimension                    = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            rtv.Texture2DArray.MipSlice          = range.baseLevel;
            rtv.Texture2DArray.FirstArraySlice   = firstLayer;
            rtv.Texture2DArray.ArraySize         = arraySize;
        } else {
            rtv.ViewDimension       = D3D12_RTV_DIMENSION_TEXTURE2D;
            rtv.Texture2D.MipSlice  = range.baseLevel;
        }

        device.GetNativeHandle()->CreateRenderTargetView(resource.Get(), &rtv, handle);
    }

    void D3D12Image::CreateDSV(D3D12_CPU_DESCRIPTOR_HANDLE handle, const ImageSubRange &range) const
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
        dsv.Format                        = dxgiFormat;
        dsv.Flags                         = D3D12_DSV_FLAG_NONE;

        const uint32_t firstLayer = range.baseLayer;
        const uint32_t arraySize  = range.layers;
        if (samples != SampleCount::X1) {
            if (arrayLayers > 1) {
                dsv.ViewDimension                     = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                dsv.Texture2DMSArray.FirstArraySlice  = firstLayer;
                dsv.Texture2DMSArray.ArraySize        = arraySize;
            } else {
                dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
            }
        } else if (arrayLayers > 1) {
            dsv.ViewDimension                    = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            dsv.Texture2DArray.MipSlice          = range.baseLevel;
            dsv.Texture2DArray.FirstArraySlice   = firstLayer;
            dsv.Texture2DArray.ArraySize         = arraySize;
        } else {
            dsv.ViewDimension       = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsv.Texture2D.MipSlice  = range.baseLevel;
        }

        device.GetNativeHandle()->CreateDepthStencilView(resource.Get(), &dsv, handle);
    }

} // namespace sky::aurora
