//
// Aurora D3D12 SwapChain (IDXGISwapChain3).
//

#include <D3D12SwapChain.h>
#include <D3D12Device.h>
#include <D3D12Queue.h>
#include <D3D12Semaphore.h>
#include <D3D12Fence.h>
#include <D3D12Conversion.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {

    D3D12SwapChain::D3D12SwapChain(D3D12Device &dev)
        : device(dev)
    {
    }

    D3D12SwapChain::~D3D12SwapChain()
    {
        DestroySwapchain();
    }

    bool D3D12SwapChain::Init(const Descriptor &desc)
    {
        hwnd = static_cast<HWND>(desc.window);
        if (hwnd == nullptr) {
            LOG_E(TAG, "swapchain requires a valid HWND window pointer");
            return false;
        }

        auto *queue   = static_cast<D3D12Queue *>(device.GetQueue(QueueType::GRAPHICS));
        auto *factory = device.GetDXGIFactory();
        if (queue == nullptr || factory == nullptr) {
            LOG_E(TAG, "swapchain requires graphics queue + DXGI factory");
            return false;
        }

        dxgiFormat  = FromPixelFormat(desc.preferredFormat);
        pixelFormat = desc.preferredFormat;
        vsync       = (desc.preferredMode == PresentMode::VSYNC);

        DXGI_SWAP_CHAIN_DESC1 scDesc = {};
        scDesc.Width       = std::max(desc.width, 1u);
        scDesc.Height      = std::max(desc.height, 1u);
        scDesc.Format      = dxgiFormat;
        scDesc.Stereo      = FALSE;
        scDesc.SampleDesc  = {1, 0};
        scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scDesc.BufferCount = bufferCount;
        scDesc.Scaling     = DXGI_SCALING_STRETCH;
        scDesc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        scDesc.AlphaMode   = DXGI_ALPHA_MODE_UNSPECIFIED;
        scDesc.Flags       = vsync ? 0 : DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

        ComPtr<IDXGISwapChain1> sc1;
        HRESULT hr = factory->CreateSwapChainForHwnd(queue->GetNativeHandle(), hwnd, &scDesc,
                                                     nullptr, nullptr, &sc1);
        if (FAILED(hr)) {
            LOG_E(TAG, "CreateSwapChainForHwnd failed: 0x%08x", hr);
            return false;
        }
        hr = sc1.As(&swapChain);
        if (FAILED(hr)) {
            LOG_E(TAG, "query IDXGISwapChain3 failed: 0x%08x", hr);
            return false;
        }

        images.clear();
        images.reserve(bufferCount);
        for (uint32_t i = 0; i < bufferCount; ++i) {
            ComPtr<ID3D12Resource> res;
            if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&res)))) {
                LOG_E(TAG, "GetBuffer(%u) failed", i);
                return false;
            }
            auto img = std::make_unique<D3D12Image>(device);
            img->InitFromSwapChain(res, dxgiFormat);
            images.push_back(std::move(img));
        }

        extent = {std::max(desc.width, 1u), std::max(desc.height, 1u)};
        return true;
    }

    uint32_t D3D12SwapChain::AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t /*timeoutNs*/)
    {
        if (!swapChain) {
            return INVALID_INDEX;
        }

        const uint32_t index = swapChain->GetCurrentBackBufferIndex();

        auto *queue = static_cast<D3D12Queue *>(device.GetQueue(QueueType::GRAPHICS));
        if (queue == nullptr) {
            return INVALID_INDEX;
        }

        // DX12 has no native acquire-signal hook; signal the binary semaphore /
        // fence right away so the caller's submit-wait is trivially satisfied.
        if (signalSema != nullptr) {
            auto *sema  = static_cast<D3D12Semaphore *>(signalSema);
            const UINT64 value = sema->AdvanceBinarySignalValue();
            queue->GetNativeHandle()->Signal(sema->GetNativeHandle(), value);
        }
        if (fence != nullptr) {
            auto *f         = static_cast<D3D12Fence *>(fence);
            const UINT64 v  = f->BumpPendingValue();
            queue->GetNativeHandle()->Signal(f->GetNativeHandle(), v);
        }

        return index;
    }

    void D3D12SwapChain::Present(uint32_t /*imageIndex*/, uint32_t numWaitSemas, Semaphore *const *waitSemas)
    {
        if (!swapChain) {
            return;
        }

        // Wait on the render-done semaphores (signaled by the submit) before
        // presenting, so the backbuffer is finished.
        for (uint32_t i = 0; i < numWaitSemas; ++i) {
            auto *sema = static_cast<D3D12Semaphore *>(waitSemas[i]);
            if (sema == nullptr) {
                continue;
            }
            const UINT64 value = sema->GetBinaryWaitValue();
            sema->Wait(value, UINT64_MAX);
        }

        swapChain->Present(vsync ? 1 : 0, 0);
    }

    void D3D12SwapChain::Resize(uint32_t width, uint32_t height)
    {
        if (!swapChain || width == 0 || height == 0) {
            return;
        }

        images.clear(); // release wrapped buffers before ResizeBuffers
        const HRESULT hr = swapChain->ResizeBuffers(bufferCount, width, height, dxgiFormat,
                                                    vsync ? 0 : DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
        if (FAILED(hr)) {
            LOG_E(TAG, "ResizeBuffers failed: 0x%08x", hr);
            return;
        }

        images.reserve(bufferCount);
        for (uint32_t i = 0; i < bufferCount; ++i) {
            ComPtr<ID3D12Resource> res;
            if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&res)))) {
                LOG_E(TAG, "GetBuffer(%u) failed after resize", i);
                return;
            }
            auto img = std::make_unique<D3D12Image>(device);
            img->InitFromSwapChain(res, dxgiFormat);
            images.push_back(std::move(img));
        }

        extent = {width, height};
    }

    Image *D3D12SwapChain::GetImage(uint32_t index) const
    {
        if (index >= images.size()) {
            return nullptr;
        }
        return images[index].get();
    }

    SwapChainStatus D3D12SwapChain::GetStatus() const
    {
        if (!swapChain) {
            return SwapChainStatus::LOST;
        }
        const Extent2D surface = GetSurfaceSize();
        if (surface.width != extent.width || surface.height != extent.height) {
            return SwapChainStatus::OUT_OF_DATE;
        }
        return SwapChainStatus::OK;
    }

    Extent2D D3D12SwapChain::GetSurfaceSize() const
    {
        RECT rect{};
        if (hwnd != nullptr && ::GetClientRect(hwnd, &rect)) {
            return {static_cast<uint32_t>(rect.right - rect.left),
                    static_cast<uint32_t>(rect.bottom - rect.top)};
        }
        return extent;
    }

    void D3D12SwapChain::DestroySwapchain()
    {
        images.clear();
        swapChain.Reset();
    }

} // namespace sky::aurora
