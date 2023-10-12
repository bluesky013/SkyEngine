//
// Created by Zach Lee on 2022/11/5.
//

#include <dx12/Swapchain.h>
#include <dx12/Device.h>

namespace sky::dx {
    SwapChain::SwapChain(Device &dev) : DevObject(dev)
    {
    }

    bool SwapChain::Init(const Descriptor &desc)
    {
        auto *dxgiFactory = device.GetDXGIFactory();

        DXGI_SWAP_CHAIN_DESC1 swcDesc = {};

        swcDesc.Width       = desc.width;
        swcDesc.Height      = desc.height;
        swcDesc.Format      = DXGI_FORMAT_B8G8R8A8_UNORM;
        swcDesc.Stereo      = false;
        swcDesc.SampleDesc  = {1, 0};
        swcDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swcDesc.BufferCount = 3;
        swcDesc.Scaling     = DXGI_SCALING_STRETCH;
        /*
         * D3D12 must use
         * DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL : persist the contents of the back buffer after present
         * DXGI_SWAP_EFFECT_FLIP_DISCARD    : discard the contents of the back buffer after present
         */
        swcDesc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        swcDesc.AlphaMode   = DXGI_ALPHA_MODE_UNSPECIFIED;
        if (FAILED(dxgiFactory->CreateSwapChainForHwnd(device.GetGraphicsQueue()->GetNativeQueue(), reinterpret_cast<HWND>(desc.window), &swcDesc,
                                                       nullptr, nullptr, swapChain.GetAddressOf()))) {
            return false;
        }

        return true;
    }
}