//
// Created by blues on 2026/3/29.
//

#include <D3D12CommandPool.h>
#include <D3D12Conversion.h>
#include <D3D12Device.h>
#include <D3D12Fence.h>
#include <D3D12Instance.h>
#include <D3D12Queue.h>
#include <D3D12Semaphore.h>
#include <D3D12ShaderFunction.h>
#include <core/logger/Logger.h>
#include <rdg/D3D12DeviceFrameContext.h>

static const char    *TAG  = "AuroraDX12";
static const wchar_t *TAGW = L"AuroraDX12";

namespace sky::aurora {

    D3D12Device::D3D12Device(D3D12Instance &inst) : instance(inst)
    {
    }

    D3D12Device::~D3D12Device()
    {
        for (auto &q : queues) {
            q.reset();
        }
        allocator.Reset();
        device.Reset();
        adapter.Reset();
    }

    bool D3D12Device::OnInit(const DeviceInit &init)
    {
        (void)init;
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

        LOG_I(TAG, "D3D12 device created successfully");
        return true;
    }

    void D3D12Device::UpdateDeviceCaps()
    {
        capability.maxThreads       = std::max(std::thread::hardware_concurrency(), 1U);
        capability.anisotropyEnable = true;
    }

    std::string D3D12Device::GetDeviceInfo() const
    {
        char   name[256] = {};
        size_t converted = 0;
        wcstombs_s(&converted, name, sizeof(name), adapterDesc.Description, _TRUNCATE);
        return name;
    }

    void D3D12Device::WaitIdle() const
    {
        for (const auto &q : queues) {
            if (q) {
                const_cast<D3D12Queue *>(q.get())->WaitIdle();
            }
        }
    }

    Queue *D3D12Device::GetQueue(QueueType type)
    {
        return queues[static_cast<size_t>(type)].get();
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

        return true;
    }

    bool D3D12Device::CreateAllocator()
    {
        D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
        allocatorDesc.pDevice                 = device.Get();
        allocatorDesc.pAdapter                = adapter.Get();
        allocatorDesc.Flags                   = D3D12MA::ALLOCATOR_FLAG_NONE;

        D3D12MA::Allocator *pAllocator = nullptr;
        const HRESULT       hr         = D3D12MA::CreateAllocator(&allocatorDesc, &pAllocator);
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
        const D3D12_COMMAND_LIST_TYPE listTypes[] = {
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            D3D12_COMMAND_LIST_TYPE_COMPUTE,
            D3D12_COMMAND_LIST_TYPE_COPY,
        };

        for (size_t i = 0; i < queues.size(); ++i) {
            D3D12_COMMAND_QUEUE_DESC desc = {};
            desc.Type                     = listTypes[i];
            desc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
            desc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
            desc.NodeMask                 = 0;

            ComPtr<ID3D12CommandQueue> raw;
            HRESULT                    hr = device->CreateCommandQueue(&desc, IID_PPV_ARGS(&raw));
            if (FAILED(hr)) {
                LOG_E(TAG, "failed to create command queue %zu, HRESULT: 0x%08x", i, hr);
                return false;
            }

            auto q = std::make_unique<D3D12Queue>(*this, static_cast<QueueType>(i), std::move(raw));
            if (!q->Init()) {
                return false;
            }
            queues[i] = std::move(q);
        }

        LOG_I(TAG, "command queues created (graphics, compute, transfer)");
        return true;
    }

    D3D12_COMMAND_LIST_TYPE D3D12Device::ToCommandListType(QueueType type)
    {
        switch (type) {
        case QueueType::COMPUTE: return D3D12_COMMAND_LIST_TYPE_COMPUTE;
        case QueueType::TRANSFER: return D3D12_COMMAND_LIST_TYPE_COPY;
        default: return D3D12_COMMAND_LIST_TYPE_DIRECT;
        }
    }

    CommandPool *D3D12Device::CreateCommandPool(QueueType type)
    {
        auto *pool = new D3D12CommandPool(*this, ToCommandListType(type));
        if (!pool->Init()) {
            delete pool;
            return nullptr;
        }
        return pool;
    }

    DeviceFrameContext *D3D12Device::CreateFrameContext(const DeviceFrameContextInitInfo &info)
    {
        return new D3D12DeviceFrameContext(this, info);
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
        support.Format                            = dxgiFormat;

        HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support));
        if (FAILED(hr)) {
            return {};
        }

        const auto              s1 = support.Support1;
        const auto              s2 = support.Support2;
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

    D3D12Context::D3D12Context(D3D12Device &dev, QueueType queue) : device(dev)
    {
        pool = std::make_unique<D3D12CommandPool>(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
        pool->Init();
    }

    void D3D12Context::OnAttach(uint32_t threadIndex)
    {
    }

    void D3D12Context::OnDetach()
    {
        pool = nullptr;
    }

} // namespace sky::aurora
