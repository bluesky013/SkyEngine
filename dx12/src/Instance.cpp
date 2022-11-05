//
// Created by Zach Lee on 2022/11/5.
//

#include <dx12/Instance.h>
#include <core/logger/Logger.h>

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

    Instance *Instance::Create(const Descriptor &des)
    {
        auto instance = new Instance();
        if (!instance->Init(des)) {
            delete instance;
            instance = nullptr;
        }
        return instance;
    }

    void Instance::Destroy(Instance *instance)
    {
        if (instance != nullptr) {
            delete instance;
        }
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
}