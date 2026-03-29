//
// Created by blues on 2026/3/29.
//

#include <D3D12Instance.h>
#include <D3D12Device.h>
#include <core/logger/Logger.h>
#include <core/platform/Platform.h>

static const char  *TAG  = "AuroraDX12";
static const wchar_t *TAGW = L"AuroraDX12";

namespace sky::aurora {

    enum class VendorID : UINT {
        Unknown = 0,
        INTEL   = 0x8086,
        NVIDIA  = 0x10de,
        AMD     = 0x1002,
    };

    D3D12Instance::~D3D12Instance()
    {
    }

    bool D3D12Instance::Init(const Instance::Descriptor &desc)
    {
        enableDebugLayer = desc.enableDebugLayer;

        if (!CreateFactory(desc)) {
            return false;
        }
        if (!EnumAdapters()) {
            return false;
        }
        return true;
    }

    Device *D3D12Instance::CreateDevice()
    {
        if (adapters.empty()) {
            LOG_E(TAG, "no adapters available");
            return nullptr;
        }

        auto *device = new D3D12Device(*this);
        if (!device->Init()) {
            delete device;
            return nullptr;
        }
        return device;
    }

    bool D3D12Instance::CreateFactory(const Instance::Descriptor &desc)
    {
        UINT dxgiFlags = 0;

        if (enableDebugLayer) {
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
                debugController->EnableDebugLayer();
                dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
                LOG_I(TAG, "D3D12 debug layer enabled");
            } else {
                LOG_W(TAG, "failed to enable D3D12 debug layer");
            }
        }

        HRESULT hr = CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&dxgiFactory));
        if (FAILED(hr)) {
            LOG_E(TAG, "failed to create DXGIFactory, HRESULT: 0x%08x", hr);
            return false;
        }
        LOG_I(TAG, "DXGIFactory created");
        return true;
    }

    bool D3D12Instance::EnumAdapters()
    {
        IDXGIAdapter1 *dxgiAdapter = nullptr;
        for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc = {};
            dxgiAdapter->GetDesc1(&desc);

            // skip software adapters
            if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0u) {
                dxgiAdapter->Release();
                continue;
            }

            LOGW_I(TAGW, L"found adapter: %ls (vendor: 0x%x)", desc.Description, desc.VendorId);

            // prefer discrete GPU (NVIDIA / AMD first)
            if (desc.VendorId == static_cast<UINT>(VendorID::AMD) ||
                desc.VendorId == static_cast<UINT>(VendorID::NVIDIA)) {
                adapters.insert(adapters.begin(), ComPtr<IDXGIAdapter1>(dxgiAdapter));
            } else {
                adapters.emplace_back(dxgiAdapter);
            }
        }

        if (adapters.empty()) {
            LOG_E(TAG, "no D3D12-capable adapter found");
            return false;
        }
        return true;
    }

} // namespace sky::aurora

extern "C" SKY_EXPORT sky::aurora::Instance::Impl *CreateInstance()
{
    return new sky::aurora::D3D12Instance();
}
