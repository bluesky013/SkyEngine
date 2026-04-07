//
// Created by blues on 2026/3/29.
//

#include <D3D12Device.h>
#include <D3D12Instance.h>
#include <D3D12CommandPool.h>
#include <D3D12Fence.h>
#include <D3D12Semaphore.h>
#include <D3D12ShaderFunction.h>
#include <D3D12Conversion.h>
#include <core/logger/Logger.h>

static const char  *TAG  = "AuroraDX12";
static const wchar_t *TAGW = L"AuroraDX12";

namespace sky::aurora {

    D3D12Device::D3D12Device(D3D12Instance &inst)
        : instance(inst)
    {
    }

    D3D12Device::~D3D12Device()
    {
        if (fence) {
            fence.Reset();
        }
        graphicsQueue.Reset();
        computeQueue.Reset();
        transferQueue.Reset();
        allocator.Reset();
        device.Reset();
        adapter.Reset();
    }

    bool D3D12Device::OnInit(const DeviceInit& init)
    {
        adapter = instance.GetAdapter(0);
        if (!adapter) {
            LOG_E(TAG, "no adapter available");
            return false;
        }

        adapter->GetDesc1(&adapterDesc);
        LOGW_I(TAGW, L"initializing device on: %ls", adapterDesc.Description);

        if (!CreateDevice()) {
            return false;
        }
        if (!CreateAllocator()) {
            return false;
        }
        if (!CreateCommandQueues()) {
            return false;
        }

        capability.maxThreads = init.parallelContextNum;

        LOG_I(TAG, "D3D12 device created successfully");
        return true;
    }

    std::string D3D12Device::GetDeviceInfo() const
    {
        char name[256] = {};
        size_t converted = 0;
        wcstombs_s(&converted, name, sizeof(name), adapterDesc.Description, _TRUNCATE);
        return name;
    }

    void D3D12Device::WaitIdle() const
    {
        if (!device || !fence) {
            return;
        }

        static UINT64 fenceValue = 0;
        ++fenceValue;

        auto waitOnQueue = [&](const ComPtr<ID3D12CommandQueue> &queue) {
            if (!queue) {
                return;
            }
            queue->Signal(fence.Get(), fenceValue);
            if (fence->GetCompletedValue() < fenceValue) {
                HANDLE event = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
                if (event != nullptr) {
                    fence->SetEventOnCompletion(fenceValue, event);
                    ::WaitForSingleObject(event, INFINITE);
                    ::CloseHandle(event);
                }
            }
            ++fenceValue;
        };

        waitOnQueue(graphicsQueue);
        waitOnQueue(computeQueue);
        waitOnQueue(transferQueue);
    }

    bool D3D12Device::CreateDevice()
    {
        HRESULT hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
        if (FAILED(hr)) {
            LOG_E(TAG, "failed to create D3D12 device, HRESULT: 0x%08x", hr);
            return false;
        }

        // enable info queue for debug builds
        if (instance.IsDebugEnabled()) {
            ComPtr<ID3D12InfoQueue> infoQueue;
            if (SUCCEEDED(device.As(&infoQueue))) {
                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            }
        }

        // create fence for synchronization
        hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        if (FAILED(hr)) {
            LOG_E(TAG, "failed to create fence, HRESULT: 0x%08x", hr);
            return false;
        }

        return true;
    }

    bool D3D12Device::CreateAllocator()
    {
        D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
        allocatorDesc.pDevice  = device.Get();
        allocatorDesc.pAdapter = adapter.Get();
        allocatorDesc.Flags    = D3D12MA::ALLOCATOR_FLAG_NONE;

        D3D12MA::Allocator *pAllocator = nullptr;
        const HRESULT hr = D3D12MA::CreateAllocator(&allocatorDesc, &pAllocator);
        if (FAILED(hr)) {
            LOG_E(TAG, "D3D12MA::CreateAllocator failed: 0x%08x", hr);
            return false;
        }
        allocator.Attach(pAllocator);

        LOG_I(TAG, "D3D12MA allocator created");
        return true;
    }

    bool D3D12Device::CreateCommandQueues()
    {
        // graphics queue
        D3D12_COMMAND_QUEUE_DESC graphicsDesc = {};
        graphicsDesc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
        graphicsDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        graphicsDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        graphicsDesc.NodeMask = 0;

        HRESULT hr = device->CreateCommandQueue(&graphicsDesc, IID_PPV_ARGS(&graphicsQueue));
        if (FAILED(hr)) {
            LOG_E(TAG, "failed to create graphics queue, HRESULT: 0x%08x", hr);
            return false;
        }

        // compute queue
        D3D12_COMMAND_QUEUE_DESC computeDesc = {};
        computeDesc.Type     = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        computeDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        computeDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        computeDesc.NodeMask = 0;

        hr = device->CreateCommandQueue(&computeDesc, IID_PPV_ARGS(&computeQueue));
        if (FAILED(hr)) {
            LOG_E(TAG, "failed to create compute queue, HRESULT: 0x%08x", hr);
            return false;
        }

        // transfer (copy) queue
        D3D12_COMMAND_QUEUE_DESC transferDesc = {};
        transferDesc.Type     = D3D12_COMMAND_LIST_TYPE_COPY;
        transferDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        transferDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        transferDesc.NodeMask = 0;

        hr = device->CreateCommandQueue(&transferDesc, IID_PPV_ARGS(&transferQueue));
        if (FAILED(hr)) {
            LOG_E(TAG, "failed to create transfer queue, HRESULT: 0x%08x", hr);
            return false;
        }

        LOG_I(TAG, "command queues created (graphics, compute, transfer)");
        return true;
    }

    D3D12_COMMAND_LIST_TYPE D3D12Device::ToCommandListType(QueueType type)
    {
        switch (type) {
        case QueueType::COMPUTE:  return D3D12_COMMAND_LIST_TYPE_COMPUTE;
        case QueueType::TRANSFER: return D3D12_COMMAND_LIST_TYPE_COPY;
        default:                  return D3D12_COMMAND_LIST_TYPE_DIRECT;
        }
    }

    Fence *D3D12Device::CreateFence(const Fence::Descriptor &desc)
    {
        auto *f = new D3D12Fence(*this);
        if (!f->Init(desc)) {
            delete f;
            return nullptr;
        }
        return f;
    }

    Semaphore *D3D12Device::CreateSema(const Semaphore::Descriptor &desc)
    {
        auto *s = new D3D12Semaphore(*this);
        if (!s->Init(desc)) {
            delete s;
            return nullptr;
        }
        return s;
    }

    Buffer *D3D12Device::CreateBuffer(const Buffer::Descriptor &desc)
    {
        auto *buf = new D3D12Buffer(*this);
        if (!buf->Init(desc)) {
            delete buf;
            return nullptr;
        }
        return buf;
    }

    Image *D3D12Device::CreateImage(const Image::Descriptor &desc)
    {
        auto *img = new D3D12Image(*this);
        if (!img->Init(desc)) {
            delete img;
            return nullptr;
        }
        return img;
    }

    Sampler *D3D12Device::CreateSampler(const Sampler::Descriptor &desc)
    {
        auto *smp = new D3D12Sampler(*this);
        if (!smp->Init(desc)) {
            delete smp;
            return nullptr;
        }
        return smp;
    }

    ShaderFunction *D3D12Device::CreateShaderFunction(const ShaderFunction::Descriptor &desc)
    {
        auto *fn = new D3D12ShaderFunction(*this);
        if (!fn->Init(desc)) {
            delete fn;
            return nullptr;
        }
        return fn;
    }

    Shader *D3D12Device::CreateShader(const Shader::Descriptor &desc)
    {
        auto *shader = new D3D12Shader(*this);
        if (!shader->Init(desc)) {
            delete shader;
            return nullptr;
        }
        return shader;
    }

    PixelFormatFeatureFlags D3D12Device::GetFormatFeatureFlags(PixelFormat format) const
    {
        const DXGI_FORMAT dxgiFormat = FromPixelFormat(format);
        if (dxgiFormat == DXGI_FORMAT_UNKNOWN) {
            return {};
        }

        D3D12_FEATURE_DATA_FORMAT_SUPPORT support = {};
        support.Format = dxgiFormat;

        HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support));
        if (FAILED(hr)) {
            return {};
        }

        const auto s1 = support.Support1;
        const auto s2 = support.Support2;
        PixelFormatFeatureFlags result;

        if (s1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) {
            result |= PixelFormatFeatureFlagBit::COLOR;
        }
        if (s1 & D3D12_FORMAT_SUPPORT1_BLENDABLE) {
            result |= PixelFormatFeatureFlagBit::BLEND;
        }
        if (s1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL) {
            result |= PixelFormatFeatureFlagBit::DEPTH_STENCIL;
        }
        if (s1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) {
            result |= PixelFormatFeatureFlagBit::SAMPLE;
        }
        if (s1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE_COMPARISON) {
            result |= PixelFormatFeatureFlagBit::SAMPLE_FILTER;
        }
        if (s1 & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) {
            result |= PixelFormatFeatureFlagBit::STORAGE;
        }
        if (s2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_ADD) {
            result |= PixelFormatFeatureFlagBit::STORAGE_ATOMIC;
        }

        return result;
    }

    ThreadContext* D3D12Device::CreateAsyncContext()
    {
        return new D3D12Context(*this);
    }

    void D3D12Context::OnAttach(uint32_t threadIndex)
    {
        pool = std::make_unique<D3D12CommandPool>(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
        pool->Init();
    }

    void D3D12Context::OnDetach()
    {
        pool = nullptr;
    }

} // namespace sky::aurora
