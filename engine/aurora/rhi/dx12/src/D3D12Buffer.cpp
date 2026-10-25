//
// Created on 2026/04/02.
//

#include <D3D12Buffer.h>

#include <D3D12Conversion.h>
#include <D3D12Device.h>
#include <core/logger/Logger.h>

#include <vector>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {
    const Device *D3D12Buffer::GetDevice() const noexcept
    {
        return &device;
    }

    D3D12Buffer::D3D12Buffer(D3D12Device &dev) : device(dev)
    {
    }

    D3D12Buffer::~D3D12Buffer()
    {
        resource.Reset();
        allocation.Reset();
    }

    bool D3D12Buffer::Init(const Descriptor &desc)
    {
        descriptor = desc;
#if SKY_ENABLE_RESOURCE_NAME
        descriptor.name = nullptr;
#endif
        size = desc.size;
        if (size == 0) {
            LOG_E(TAG, "buffer size must be non-zero");
            return false;
        }

        D3D12MA::ALLOCATION_DESC allocDesc = {};
        allocDesc.HeapType                 = FromMemoryType(desc.memory);

        D3D12_RESOURCE_DESC resDesc = {};
        resDesc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
        resDesc.Alignment           = 0;
        resDesc.Width               = size;
        resDesc.Height              = 1;
        resDesc.DepthOrArraySize    = 1;
        resDesc.MipLevels           = 1;
        resDesc.Format              = DXGI_FORMAT_UNKNOWN;
        resDesc.SampleDesc          = {1, 0};
        resDesc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resDesc.Flags               = FromBufferUsageFlags(desc.usage);
        // Upload storage is read-only SRV data; upload heaps cannot carry UAV flags.
        if (allocDesc.HeapType == D3D12_HEAP_TYPE_UPLOAD) { resDesc.Flags = D3D12_RESOURCE_FLAG_NONE; }

        const D3D12_RESOURCE_STATES initState = (allocDesc.HeapType == D3D12_HEAP_TYPE_UPLOAD)     ? D3D12_RESOURCE_STATE_GENERIC_READ
                                                : (allocDesc.HeapType == D3D12_HEAP_TYPE_READBACK) ? D3D12_RESOURCE_STATE_COPY_DEST
                                                                                                   : D3D12_RESOURCE_STATE_COMMON;

        D3D12MA::Allocation *pAlloc = nullptr;
        const HRESULT        hr     = createResult =
            device.GetAllocator()->CreateResource(&allocDesc, &resDesc, initState, nullptr, &pAlloc, IID_PPV_ARGS(resource.GetAddressOf()));

        if (FAILED(hr)) {
            LOG_E(TAG, "D3D12MA CreateResource (buffer) failed: 0x%08x", hr);
            return false;
        }
        allocation.Attach(pAlloc);

#if SKY_ENABLE_RESOURCE_NAME
        if (desc.name != nullptr && resource) {
            std::wstring wideName;
            for (const char *c = desc.name; *c != '\0'; ++c) {
                wideName.push_back(static_cast<wchar_t>(*c));
            }
            resource->SetName(wideName.c_str());
        }
#endif

        return true;
    }

    uint8_t *D3D12Buffer::Map()
    {
        if (!resource) {
            return nullptr;
        }
        void             *data      = nullptr;
        const D3D12_RANGE readRange = {0, 0}; // no CPU read
        const HRESULT     hr = mapResult = resource->Map(0, &readRange, &data);
        if (FAILED(hr)) {
            LOG_E(TAG, "buffer Map failed: 0x%08x (device removed reason 0x%08x)", static_cast<unsigned>(hr),
                  static_cast<unsigned>(device.GetNativeHandle()->GetDeviceRemovedReason()));
            return nullptr;
        }
        return static_cast<uint8_t *>(data);
    }

    void D3D12Buffer::UnMap()
    {
        if (resource) {
            resource->Unmap(0, nullptr);
        }
    }

    void D3D12Buffer::CreateCBV(D3D12_CPU_DESCRIPTOR_HANDLE handle, uint64_t offset, uint64_t range) const
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv = {};
        cbv.BufferLocation                  = resource->GetGPUVirtualAddress() + offset;
        cbv.SizeInBytes                     = static_cast<UINT>(range == 0 ? size : range);
        cbv.SizeInBytes                     = (cbv.SizeInBytes + 255) & ~255u;
        device.GetNativeHandle()->CreateConstantBufferView(&cbv, handle);
    }

    std::optional<FrameError> D3D12Buffer::CheckStorageRange(const ShaderResource &binding, uint64_t offset, uint64_t range) const
    {
        if (!resource || !descriptor.usage.TestBit(BufferUsageFlagBit::STORAGE) || descriptor.memory != MemoryType::GPU_ONLY || offset >= size ||
            !range || range > size - offset)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Storage buffer view exceeds its backing or usage/heap"};
        if (binding.access != ShaderResourceAccess::READ_ONLY && binding.access != ShaderResourceAccess::READ_WRITE)
            return FrameError{FrameErrorCode::UNSUPPORTED, "Unknown storage buffer access"};
        uint32_t stride = 0;
        if (binding.bufferKind == ShaderBufferKind::RAW) {
            if (binding.elementStride || offset % D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Raw buffer views require no stride and a 16-byte aligned address"};
            stride = sizeof(uint32_t);
        } else if (binding.bufferKind == ShaderBufferKind::STRUCTURED) {
            if (!binding.elementStride || binding.elementStride % 4 || binding.elementStride > D3D12_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES)
                return FrameError{FrameErrorCode::UNSUPPORTED, "Unsupported structured buffer stride"};
            stride = binding.elementStride;
        } else {
            return FrameError{FrameErrorCode::UNSUPPORTED, "Only raw and structured buffer views are supported"};
        }
        if (offset % stride || range % stride || range / stride > UINT32_MAX)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Storage buffer range is not a bounded complete element range"};
        return {};
    }

    std::optional<FrameError>
    D3D12Buffer::CreateStorageView(D3D12_CPU_DESCRIPTOR_HANDLE handle, const ShaderResource &binding, uint64_t offset, uint64_t range) const
    {
        if (!handle.ptr)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Storage buffer descriptor is missing"};
        if (const auto error = CheckStorageRange(binding, offset, range))
            return error;
        const bool raw   = binding.bufferKind == ShaderBufferKind::RAW;
        const auto unit  = raw ? sizeof(uint32_t) : binding.elementStride;
        const auto count = static_cast<UINT>(range / unit);
        if (binding.access == ShaderResourceAccess::READ_ONLY) {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
            srv.Format                     = raw ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
            srv.ViewDimension              = D3D12_SRV_DIMENSION_BUFFER;
            srv.Shader4ComponentMapping    = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv.Buffer.FirstElement        = offset / unit;
            srv.Buffer.NumElements         = count;
            srv.Buffer.StructureByteStride = raw ? 0 : binding.elementStride;
            srv.Buffer.Flags               = raw ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;
            device.GetNativeHandle()->CreateShaderResourceView(resource.Get(), &srv, handle);
        } else {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
            uav.Format                     = raw ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
            uav.ViewDimension              = D3D12_UAV_DIMENSION_BUFFER;
            uav.Buffer.FirstElement        = offset / unit;
            uav.Buffer.NumElements         = count;
            uav.Buffer.StructureByteStride = raw ? 0 : binding.elementStride;
            uav.Buffer.Flags               = raw ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE;
            device.GetNativeHandle()->CreateUnorderedAccessView(resource.Get(), nullptr, &uav, handle);
        }
        return {};
    }

} // namespace sky::aurora
