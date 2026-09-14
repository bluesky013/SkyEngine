//
// Aurora D3D12 SwapChain (IDXGISwapChain3).
//

#pragma once

#include <aurora/rhi/SwapChain.h>
#include <D3D12Image.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <windows.h>

#include <memory>
#include <vector>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;

    class D3D12SwapChain : public SwapChain {
    public:
        explicit D3D12SwapChain(D3D12Device &dev);
        ~D3D12SwapChain() override;

        bool Init(const Descriptor &desc);

        uint32_t    AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t timeoutNs) override;
        void        Present(uint32_t imageIndex, uint32_t numWaitSemas, Semaphore *const *waitSemas) override;
        void        Resize(uint32_t width, uint32_t height) override;
        Image*      GetImage(uint32_t index) const override;
        uint32_t    GetImageCount() const override { return static_cast<uint32_t>(images.size()); }
        PixelFormat GetFormat() const override { return pixelFormat; }
        Extent2D    GetExtent() const override { return extent; }
        SwapChainStatus GetStatus() const override;
        Extent2D    GetSurfaceSize() const override;

    private:
        void DestroySwapchain();

        D3D12Device              &device;
        HWND                      hwnd = nullptr;
        ComPtr<IDXGISwapChain3>   swapChain;
        std::vector<std::unique_ptr<D3D12Image>> images;
        DXGI_FORMAT               dxgiFormat  = DXGI_FORMAT_B8G8R8A8_UNORM;
        PixelFormat               pixelFormat = PixelFormat::BGRA8_UNORM;
        Extent2D                  extent      = {1, 1};
        uint32_t                  bufferCount = 3;
        bool                      vsync       = false;
    };

} // namespace sky::aurora
