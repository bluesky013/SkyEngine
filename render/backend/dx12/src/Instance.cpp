//
// Created by Zach Lee on 2022/11/5.
//

#include "core/platform/Platform.h"
#include <core/logger/Logger.h>
#include <dx12/Instance.h>

static const char* TAG = "DX12";
static const wchar_t * TAGW = L"DX12";

namespace sky::dx {

    enum class VendorID : UINT {
        Unknown = 0,
        INTEL   = 0x8086,
        NVIDIA  = 0x10de,
        AMD     = 0x1002,
    };

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

            if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0u) {
                continue;
            }
            if (desc.VendorId == static_cast<UINT>(VendorID::AMD) || desc.VendorId == static_cast<UINT>(VendorID::NVIDIA)) {
                adapters.insert(adapters.begin(), dxgiAdapter);
            } else {
                adapters.emplace_back(dxgiAdapter);
            }
        }

        return true;
    }

    Device *Instance::CreateDevice(const Device::Descriptor &desc)
    {
        if (adapters.empty()) {
            return nullptr;
        }

        const auto &adapter = adapters.front();
        DXGI_ADAPTER_DESC1 adapterDesc;
        adapter->GetDesc1(&adapterDesc);
        PrintDevice(adapterDesc);

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