//
// Created on 2026/04/02.
//

#include "D3D12Buffer.h"
#include "D3D12Device.h"
#include "D3D12Conversion.h"
#include <core/logger/Logger.h>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {

    D3D12Buffer::D3D12Buffer(D3D12Device &dev)
        : device(dev)
    {
    }

    bool D3D12Buffer::Init(const Descriptor &desc)
    {
        size = desc.size;
        if (size == 0) {
            LOG_E(TAG, "buffer size must be non-zero");
            return false;
        }

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = FromMemoryType(desc.memory);

        D3D12_RESOURCE_DESC resDesc = {};
        resDesc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        resDesc.Alignment        = 0;
        resDesc.Width            = size;
        resDesc.Height           = 1;
        resDesc.DepthOrArraySize = 1;
        resDesc.MipLevels        = 1;
        resDesc.Format           = DXGI_FORMAT_UNKNOWN;
        resDesc.SampleDesc       = {1, 0};
        resDesc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resDesc.Flags            = FromBufferUsageFlags(desc.usage);

        const D3D12_RESOURCE_STATES initState =
            (heapProps.Type == D3D12_HEAP_TYPE_UPLOAD)   ? D3D12_RESOURCE_STATE_GENERIC_READ :
            (heapProps.Type == D3D12_HEAP_TYPE_READBACK)  ? D3D12_RESOURCE_STATE_COPY_DEST :
            D3D12_RESOURCE_STATE_COMMON;

        const HRESULT hr = device.GetNativeHandle()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            initState,
            nullptr,
            IID_PPV_ARGS(resource.GetAddressOf()));

        if (FAILED(hr)) {
            LOG_E(TAG, "CreateCommittedResource (buffer) failed: 0x%08x", hr);
            return false;
        }

        return true;
    }

    uint8_t *D3D12Buffer::Map()
    {
        if (!resource) {
            return nullptr;
        }
        void *data = nullptr;
        const D3D12_RANGE readRange = {0, 0}; // no CPU read
        const HRESULT hr = resource->Map(0, &readRange, &data);
        if (FAILED(hr)) {
            LOG_E(TAG, "buffer Map failed: 0x%08x", hr);
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

} // namespace sky::aurora
