//
// Created by blues on 2026/3/29.
//

#pragma once

#include <D3D12CommandPool.h>
#include <D3D12Buffer.h>
#include <D3D12Image.h>
#include <D3D12Sampler.h>

#include <aurora/rhi/Device.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <vector>
#include <string>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;
    class D3D12Instance;

    struct D3D12Context : ThreadContext {
        explicit D3D12Context(D3D12Device& dev) : device(dev) {}

        void OnAttach(uint32_t threadIndex) override;
        void OnDetach() override;

        D3D12Device& device;
        std::unique_ptr<D3D12CommandPool> pool;
    };

    class D3D12Device : public Device {
    public:
        explicit D3D12Device(D3D12Instance &inst);
        ~D3D12Device() override;

        Fence *CreateFence(const Fence::Descriptor &desc) override;
        Semaphore *CreateSema(const Semaphore::Descriptor &desc) override;

        Buffer* CreateBuffer(const Buffer::Descriptor &desc) override;
        Image* CreateImage(const Image::Descriptor &desc) override;
        Sampler* CreateSampler(const Sampler::Descriptor &desc) override;
        ResourceGroup* CreateSampler(const ResourceGroup::Descriptor &desc) override { return nullptr; }
        SwapChain* CreateSwapChain(const SwapChain::Descriptor &desc) override { return nullptr; }

        ShaderFunction* CreateShaderFunction(const ShaderFunction::Descriptor &desc) override { return nullptr; }
        Shader* CreateShader(const Shader::Descriptor &desc) override { return nullptr; }
        GraphicsPipeline* CreatePipelineState(const GraphicsPipeline::Descriptor &desc) override { return nullptr; }
        ComputePipeline* CreatePipelineState(const ComputePipeline::Descriptor &desc) override { return nullptr; }

        ID3D12Device  *GetNativeHandle() const { return device.Get(); }
        IDXGIAdapter1 *GetAdapter() const { return adapter.Get(); }

    private:
        ThreadContext* CreateAsyncContext() override;
        bool OnInit(const DeviceInit& init) override;
        std::string GetDeviceInfo() const override;
        void WaitIdle() const override;

        bool CreateDevice();
        bool CreateCommandQueues();
        static D3D12_COMMAND_LIST_TYPE ToCommandListType(QueueType type);

        D3D12Instance &instance;

        ComPtr<IDXGIAdapter1>      adapter;
        ComPtr<ID3D12Device>       device;
        ComPtr<ID3D12CommandQueue> graphicsQueue;
        ComPtr<ID3D12CommandQueue> computeQueue;
        ComPtr<ID3D12CommandQueue> transferQueue;
        ComPtr<ID3D12Fence>        fence;

        DXGI_ADAPTER_DESC1 adapterDesc = {};
    };

} // namespace sky::aurora
