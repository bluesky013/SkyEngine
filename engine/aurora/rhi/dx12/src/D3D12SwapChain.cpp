//
// Aurora D3D12 SwapChain (IDXGISwapChain3).
//

#include <D3D12Conversion.h>
#include <D3D12Device.h>
#include <D3D12Fence.h>
#include <D3D12Queue.h>
#include <D3D12Semaphore.h>
#include <D3D12SwapChain.h>
#include <algorithm>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {

    D3D12SwapChain::D3D12SwapChain(D3D12Device &dev)
        : device(dev)
    {
    }

    D3D12SwapChain::~D3D12SwapChain()
    {
        if (std::any_of(presentationFrames.begin(), presentationFrames.end(), [](const auto &frame) { return frame.serial; }))
            device.GetQueue(QueueType::GRAPHICS)->WaitIdle();
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
        ComPtr<IDXGIFactory5> factory5;
        BOOL                  tearing = FALSE;
        allowTearing = !vsync && SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory5))) &&
                       SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof(tearing))) && tearing;

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
        scDesc.Flags                 = allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

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
        if (checkedProtocol)
            return INVALID_INDEX;
        directProtocol = true;
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
        if (checkedProtocol)
            return;
        directProtocol = true;
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
        if (checkedProtocol)
            return;
        directProtocol = true;
        if (!swapChain || width == 0 || height == 0) {
            return;
        }

        images.clear(); // release wrapped buffers before ResizeBuffers
        const HRESULT hr = swapChain->ResizeBuffers(bufferCount, width, height, dxgiFormat, allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
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

    Device *D3D12SwapChain::GetDevice() const noexcept
    {
        return &device;
    }

    SurfaceQueryResult D3D12SwapChain::QuerySurface() const noexcept
    {
        if (!swapChain || !IsWindow(hwnd))
            return {{PresentationStatus::SURFACE_LOST}, {}};
        const auto removed = device.GetFunctions().removed(device.GetNativeHandle());
        if (FAILED(removed))
            return {{PresentationStatus::FAILED, D3D12Error(removed)}, {}};
        RECT rect{};
        if (!GetClientRect(hwnd, &rect))
            return {{PresentationStatus::FAILED, {GpuErrorCode::NATIVE_ERROR, GetLastError()}}, {}};
        if (IsIconic(hwnd) || rect.right <= rect.left || rect.bottom <= rect.top)
            return {{PresentationStatus::BUSY}, {}};
        DXGI_SWAP_CHAIN_DESC1 descriptor{};
        const auto            queried = swapChain->GetDesc1(&descriptor);
        if (FAILED(queried))
            return {{PresentationStatus::FAILED, D3D12Error(queried)}, {}};
        if (descriptor.Format != dxgiFormat || descriptor.BufferCount != bufferCount)
            return {{PresentationStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}}, {}};
        SurfaceConfiguration configuration{{static_cast<uint32_t>(rect.right - rect.left), static_cast<uint32_t>(rect.bottom - rect.top)},
                                           pixelFormat,
                                           SurfaceTransform::IDENTITY,
                                           surfaceRevision};
        const bool           same =
            configuration.extent.width == descriptor.Width && configuration.extent.height == descriptor.Height && images.size() == bufferCount;
        return {{same ? PresentationStatus::READY : PresentationStatus::OUT_OF_DATE}, configuration};
    }

    SwapChainAcquireResult D3D12SwapChain::TryAcquireFrame(uint64_t timeoutNs) noexcept
    {
        if (directProtocol || timeoutNs > 1'000'000 || presentationSerial == UINT64_MAX)
            return {{PresentationStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}}, {}};
        checkedProtocol = true;
        if (!swapChain || images.size() != bufferCount)
            return {{PresentationStatus::SURFACE_LOST}, {}};
        const auto queried = QuerySurface();
        if (queried.status != PresentationStatus::READY)
            return {{queried.status, queried.error}, {}};
        const auto index = swapChain->GetCurrentBackBufferIndex();
        if (std::any_of(presentationFrames.begin(), presentationFrames.end(),
                        [index](const auto &frame) { return frame.serial && frame.imageIndex == index; }))
            return {{PresentationStatus::BUSY}, {}};
        auto found = std::find_if(presentationFrames.begin(), presentationFrames.end(), [](const auto &frame) { return !frame.serial; });
        if (found == presentationFrames.end()) {
            if (presentationFrames.size() >= images.size())
                return {{PresentationStatus::BUSY}, {}};
            presentationFrames.emplace_back();
            found = presentationFrames.end() - 1;
        }
        auto &frame = *found;
        if (!frame.presentFence)
            frame.presentFence = FencePtr(device.CreateFence({false}));
        if (!frame.presentFence)
            return {{PresentationStatus::FAILED, {GpuErrorCode::NATIVE_ERROR}}, {}};
        const auto reset = frame.presentFence->TryReset();
        if (!reset.Succeeded())
            return {{PresentationStatus::FAILED, reset.error}, {}};
        frame.serial     = ++presentationSerial;
        frame.imageIndex = index;
        frame.presented  = false;
        frame.error      = {};
        return {{PresentationStatus::READY}, {frame.serial, index}};
    }

    PresentationResult D3D12SwapChain::TryPresentFrame(uint64_t serial, bool gpuComplete) noexcept
    {
        const auto found = std::find_if(presentationFrames.begin(), presentationFrames.end(),
                                        [serial](const auto &frame) { return serial && frame.serial == serial; });
        if (found == presentationFrames.end() || found->presented || found->imageIndex != swapChain->GetCurrentBackBufferIndex())
            return {PresentationStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}};
        if (!gpuComplete)
            return {PresentationStatus::BUSY};
        auto      &frame = *found;
        const auto result =
            device.GetFunctions().present(swapChain.Get(), vsync ? 1 : 0, DXGI_PRESENT_DO_NOT_WAIT | (allowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0));
        if (result == DXGI_ERROR_WAS_STILL_DRAWING)
            return {PresentationStatus::BUSY};
        if (FAILED(result))
            return {PresentationStatus::FAILED, D3D12Error(result)};
        frame.presented   = true;
        auto      *fence  = static_cast<D3D12Fence *>(frame.presentFence.Get());
        const auto signal = device.GetFunctions().signal(static_cast<D3D12Queue *>(device.GetQueue(QueueType::GRAPHICS))->GetNativeHandle(),
                                                         fence->GetNativeHandle(), fence->GetPendingValue());
        if (FAILED(signal)) {
            frame.error = D3D12Error(signal);
            return {PresentationStatus::FAILED, frame.error};
        }
        return {PresentationStatus::READY};
    }

    FenceQueryResult D3D12SwapChain::RetireFrame(uint64_t serial, bool abandon) noexcept
    {
        const auto found = std::find_if(presentationFrames.begin(), presentationFrames.end(),
                                        [serial](const auto &frame) { return serial && frame.serial == serial; });
        if (found == presentationFrames.end() || found->presented == abandon)
            return {FenceStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}};
        auto &frame = *found;
        if (frame.error.code != GpuErrorCode::NONE)
            return {FenceStatus::FAILED, frame.error};
        const auto result = abandon ? FenceQueryResult{FenceStatus::SIGNALED} : frame.presentFence->Query();
        if (result.status == FenceStatus::SIGNALED)
            frame.serial = 0;
        return result;
    }

    PresentationResult D3D12SwapChain::TryResize(uint32_t width, uint32_t height) noexcept
    {
        if (directProtocol || surfaceRevision == UINT64_MAX)
            return {PresentationStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}};
        checkedProtocol = true;
        if (!swapChain)
            return {PresentationStatus::SURFACE_LOST};
        if (!width || !height || std::any_of(presentationFrames.begin(), presentationFrames.end(), [](const auto &frame) { return frame.serial; }))
            return {PresentationStatus::BUSY};
        images.clear();
        const auto result = device.GetFunctions().resizeBuffers(swapChain.Get(), bufferCount, width, height, dxgiFormat,
                                                                allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
        if (SUCCEEDED(result))
            extent = {width, height};
        // Resize failure leaves the old native buffers alive; rewrap them before returning.
        for (uint32_t i = 0; i < bufferCount; ++i) {
            ComPtr<ID3D12Resource> resource;
            const auto             buffer = swapChain->GetBuffer(i, IID_PPV_ARGS(&resource));
            if (FAILED(buffer)) {
                images.clear();
                return {PresentationStatus::FAILED, D3D12Error(buffer)};
            }
            auto image = std::make_unique<D3D12Image>(device);
            image->InitFromSwapChain(resource, dxgiFormat);
            images.push_back(std::move(image));
        }
        if (FAILED(result))
            return {PresentationStatus::FAILED, D3D12Error(result)};
        ++surfaceRevision;
        return {PresentationStatus::READY};
    }

    PresentationResult D3D12SwapChain::TryClose() noexcept
    {
        if (directProtocol)
            return {PresentationStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}};
        checkedProtocol = true;
        if (std::any_of(presentationFrames.begin(), presentationFrames.end(), [](const auto &frame) { return frame.serial; }))
            return {PresentationStatus::BUSY};
        presentationFrames.clear();
        DestroySwapchain();
        return {PresentationStatus::READY};
    }
} // namespace sky::aurora
