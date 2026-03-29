//
// Created by blues on 2026/3/29.
//

#pragma once

#include <aurora/rhi/Instance.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <vector>
#include <string>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;

    class D3D12Instance : public Instance::Impl {
    public:
        D3D12Instance() = default;
        ~D3D12Instance() override;

        bool Init(const Instance::Descriptor &desc) override;
        Device *CreateDevice() override;

        IDXGIFactory4 *GetDXGIFactory() const { return dxgiFactory.Get(); }
        IDXGIAdapter1 *GetAdapter(uint32_t index = 0) const { return index < adapters.size() ? adapters[index].Get() : nullptr; }
        bool           IsDebugEnabled() const { return enableDebugLayer; }

    private:
        bool CreateFactory(const Instance::Descriptor &desc);
        bool EnumAdapters();

        ComPtr<IDXGIFactory4>              dxgiFactory;
        ComPtr<ID3D12Debug>                debugController;
        std::vector<ComPtr<IDXGIAdapter1>> adapters;

        bool enableDebugLayer = false;
    };

} // namespace sky::aurora
