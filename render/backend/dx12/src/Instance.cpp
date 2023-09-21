//
// Created by Zach Lee on 2022/11/5.
//

#include "core/platform/Platform.h"
#include <core/logger/Logger.h>
#include <dx12/Instance.h>

static const char* TAG = "DX12";
static const wchar_t * TAGW = L"DX12";

namespace sky::dx {

    static void PrintDevice(DXGI_ADAPTER_DESC1 &desc)
    {
        LOG_I(TAG, "/*********Device Info Begin*********/");
        LOGW_I(TAGW, L"Device Description: %ls", desc.Description);
        LOG_I(TAG, "Device VendorId: %u", desc.VendorId);
        LOG_I(TAG, "Device DeviceId: %u", desc.DeviceId);
        LOG_I(TAG, "Device DedicatedSystemMemory: %llu", desc.DedicatedSystemMemory);
        LOG_I(TAG, "Device DedicatedVideoMemory: %llu", desc.DedicatedVideoMemory);
        LOG_I(TAG, "Device SharedSystemMemory: %llu", desc.SharedSystemMemory);
        LOG_I(TAG, "/**********Device Info End**********/");
    }

    bool Instance::Init(const Descriptor &)
    {
        if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(dxgiFactory.GetAddressOf())))) {
            return false;
        }

        IDXGIAdapter1 *dxgiAdapter = nullptr;
        for (uint32_t i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc = {};
            dxgiAdapter->GetDesc1(&desc);
            PrintDevice(desc);
            adapters.emplace_back(dxgiAdapter);
        }

        return true;
    }

    Device *Instance::CreateDevice(const Device::Descriptor &desc)
    {
        if (adapters.empty()) {
            return nullptr;
        }
        auto *device = new Device(*this);
        if (!device->Init(desc, adapters.front())) {
            delete device;
            device = nullptr;
        }
        return device;
    }

    IDXGIFactory2 *Instance::GetDXGIFactory() const
    {
        return dxgiFactory.Get();
    }
}

extern "C" SKY_EXPORT sky::rhi::Instance *CreateInstance()
{
    return new sky::dx::Instance();
}