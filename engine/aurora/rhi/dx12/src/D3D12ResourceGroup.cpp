//
// Created on 2026/09/13.
//

#include <D3D12ResourceGroup.h>
#include <D3D12Buffer.h>
#include <D3D12Device.h>
#include <D3D12Image.h>
#include <D3D12ResourceGroupLayout.h>
#include <D3D12Sampler.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {

    D3D12ResourceGroup::D3D12ResourceGroup(D3D12Device &dev)
        : device(&dev)
    {
    }

    D3D12ResourceGroup::~D3D12ResourceGroup()
    {
        if (device != nullptr) {
            auto *allocator = device->GetDescriptorAllocator();
            if (allocator != nullptr) {
                allocator->Free(allocation);
            }
        }
    }

    bool D3D12ResourceGroup::Init(const Descriptor &desc)
    {
        if (desc.layout == nullptr) {
            LOG_E(TAG, "ResourceGroup requires a non-null layout");
            return false;
        }
        layout = static_cast<D3D12ResourceGroupLayout *>(desc.layout);

        auto *allocator = device->GetDescriptorAllocator();
        if (allocator == nullptr) {
            LOG_E(TAG, "descriptor allocator is not initialized");
            return false;
        }

        if (!allocator->Allocate(layout->GetCbvSrvUavCount(), layout->GetSamplerCount(), allocation)) {
            LOG_E(TAG, "descriptor heap allocation failed");
            return false;
        }
        return true;
    }

    void D3D12ResourceGroup::Update(const std::vector<ResourceUpdateInfo> &writes)
    {
        if (writes.empty() || layout == nullptr) {
            return;
        }

        auto *allocator = device->GetDescriptorAllocator();
        if (allocator == nullptr) {
            return;
        }

        for (const auto &w : writes) {
            const uint32_t index = layout->FindBinding(w.binding);
            if (index == INVALID_INDEX) {
                continue;
            }
            const auto &binding = layout->GetBindings()[index];

            switch (w.kind) {
            case ResourceWriteKind::BUFFER: {
                auto *buf = static_cast<D3D12Buffer *>(w.buffer);
                if (buf == nullptr) {
                    break;
                }
                const uint32_t slot           = allocation.cbvSrvUavFirst + layout->GetCbvSrvUavOffset(index) + w.arrayElement;
                const auto     handle         = allocator->GetCbvSrvUavCpuHandle(slot);
                if (binding.type == DescriptorType::STORAGE_BUFFER || binding.type == DescriptorType::STORAGE_BUFFER_DYNAMIC) {
                    buf->CreateUAV(handle);
                } else if (binding.type == DescriptorType::UNIFORM_BUFFER || binding.type == DescriptorType::UNIFORM_BUFFER_DYNAMIC) {
                    buf->CreateCBV(handle, w.bufferOffset, w.bufferRange);
                } else {
                    buf->CreateSRV(handle);
                }
                break;
            }
            case ResourceWriteKind::IMAGE: {
                auto *img = static_cast<D3D12Image *>(w.image);
                if (img == nullptr) {
                    break;
                }
                const uint32_t slot   = allocation.cbvSrvUavFirst + layout->GetCbvSrvUavOffset(index) + w.arrayElement;
                const auto     handle = allocator->GetCbvSrvUavCpuHandle(slot);
                if (binding.type == DescriptorType::STORAGE_IMAGE) {
                    img->CreateUAV(handle);
                } else {
                    img->CreateSRV(handle);
                }
                break;
            }
            case ResourceWriteKind::SAMPLER: {
                auto *smp = static_cast<D3D12Sampler *>(w.sampler);
                if (smp == nullptr) {
                    break;
                }
                const uint32_t slot   = allocation.samplerFirst + layout->GetSamplerOffset(index) + w.arrayElement;
                const auto     handle = allocator->GetSamplerCpuHandle(slot);
                device->GetNativeHandle()->CreateSampler(&smp->GetNativeDesc(), handle);
                break;
            }
            case ResourceWriteKind::COMBINED_IMAGE_SAMPLER: {
                auto *img = static_cast<D3D12Image *>(w.image);
                auto *smp = static_cast<D3D12Sampler *>(w.sampler);
                if (img != nullptr) {
                    const uint32_t slot   = allocation.cbvSrvUavFirst + layout->GetCbvSrvUavOffset(index) + w.arrayElement;
                    const auto     handle = allocator->GetCbvSrvUavCpuHandle(slot);
                    img->CreateSRV(handle);
                }
                if (smp != nullptr) {
                    const uint32_t slot   = allocation.samplerFirst + layout->GetSamplerOffset(index) + w.arrayElement;
                    const auto     handle = allocator->GetSamplerCpuHandle(slot);
                    device->GetNativeHandle()->CreateSampler(&smp->GetNativeDesc(), handle);
                }
                break;
            }
            }
        }
    }

    D3D12_GPU_DESCRIPTOR_HANDLE D3D12ResourceGroup::GetCbvSrvUavGpuHandle() const
    {
        return device->GetDescriptorAllocator()->GetCbvSrvUavGpuHandle(allocation.cbvSrvUavFirst);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE D3D12ResourceGroup::GetSamplerGpuHandle() const
    {
        return device->GetDescriptorAllocator()->GetSamplerGpuHandle(allocation.samplerFirst);
    }

} // namespace sky::aurora
