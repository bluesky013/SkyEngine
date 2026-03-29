//
// Created by blues on 2026/3/29.
//

#pragma once

#include <aurora/rhi/Device.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <vector>
#include <string>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Instance;

    class D3D12Device : public Device {
    public:
        explicit D3D12Device(D3D12Instance &inst);
        ~D3D12Device() override;

        bool Init() override;
        std::string GetDeviceInfo() const override;
        void WaitIdle() const override;

        CommandPool *CreateCommandPool(QueueType type) override;

        ID3D12Device  *GetNativeHandle() const { return device.Get(); }
        IDXGIAdapter1 *GetAdapter() const { return adapter.Get(); }

    private:
        bool CreateDevice();
        bool CreateCommandQueues();
        static D3D12_COMMAND_LIST_TYPE ToCommandListType(QueueType type);

        D3D12Instance &instance;

        ComPtr<IDXGIAdapter1>      adapter;
        ComPtr<ID3D12Device>       device;
        ComPtr<ID3D12CommandQueue>  graphicsQueue;
        ComPtr<ID3D12CommandQueue>  computeQueue;
        ComPtr<ID3D12CommandQueue>  transferQueue;
        ComPtr<ID3D12Fence>        fence;

        DXGI_ADAPTER_DESC1 adapterDesc = {};
    };

} // namespace sky::aurora
