//
// Created on 2026/09/14.
//

#include <D3D12DescriptorEncoder.h>
#include <D3D12ResourceGroup.h>
#include <D3D12Buffer.h>
#include <D3D12Device.h>
#include <D3D12Image.h>
#include <D3D12Sampler.h>
#include <aurora/rhi/Core.h>

namespace sky::aurora {

    D3D12DescriptorEncoder::D3D12DescriptorEncoder(D3D12ResourceGroup &rg)
        : group(&rg)
    {
    }

    void D3D12DescriptorEncoder::WriteBuffer(uint32_t binding, Buffer *buffer,
                                             uint64_t offset, uint64_t range,
                                             uint32_t arrayElement)
    {
        auto *allocator = group->device->GetDescriptorAllocator();
        if (allocator == nullptr) {
            return;
        }

        uint32_t index = INVALID_INDEX;
        for (size_t i = 0; i < group->setResources.size(); ++i) {
            if (group->setResources[i].binding == binding) {
                index = static_cast<uint32_t>(i);
                break;
            }
        }
        if (index == INVALID_INDEX) {
            return;
        }
        const auto &res = group->setResources[index];
        auto *buf       = static_cast<D3D12Buffer *>(buffer);
        if (buf == nullptr) {
            return;
        }

        if (res.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC ||
            res.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC) {
            // record the buffer; address is rebound at bind time with offset
            for (auto &db : group->dynamicBindings) {
                if (db.binding == res.binding) {
                    db.buffer     = buf;
                    db.baseOffset = offset;
                    db.range      = range;
                    break;
                }
            }
            return;
        }

        group->mDirty = true;
        const uint32_t slot   = group->allocation.cbvSrvUavFirst + group->cbvSrvUavOffsets[index] + arrayElement;
        const auto     handle = allocator->GetCbvSrvUavCpuHandle(slot);
        if (res.type == ShaderResourceType::STORAGE_BUFFER) {
            buf->CreateUAV(handle);
        } else if (res.type == ShaderResourceType::UNIFORM_BUFFER) {
            buf->CreateCBV(handle, offset, range);
        } else {
            buf->CreateSRV(handle);
        }
    }

    void D3D12DescriptorEncoder::WriteImage(uint32_t binding, Image *image,
                                            ImageLayout /*layout*/, uint32_t arrayElement)
    {
        auto *allocator = group->device->GetDescriptorAllocator();
        if (allocator == nullptr) {
            return;
        }

        uint32_t index = INVALID_INDEX;
        for (size_t i = 0; i < group->setResources.size(); ++i) {
            if (group->setResources[i].binding == binding) {
                index = static_cast<uint32_t>(i);
                break;
            }
        }
        if (index == INVALID_INDEX) {
            return;
        }
        const auto &res = group->setResources[index];
        auto *img       = static_cast<D3D12Image *>(image);
        if (img == nullptr) {
            return;
        }

        group->mDirty = true;
        const uint32_t slot   = group->allocation.cbvSrvUavFirst + group->cbvSrvUavOffsets[index] + arrayElement;
        const auto     handle = allocator->GetCbvSrvUavCpuHandle(slot);
        if (res.type == ShaderResourceType::STORAGE_IMAGE) {
            img->CreateUAV(handle);
        } else {
            img->CreateSRV(handle);
        }
    }

    void D3D12DescriptorEncoder::WriteSampler(uint32_t binding, Sampler *sampler, uint32_t arrayElement)
    {
        auto *allocator = group->device->GetDescriptorAllocator();
        if (allocator == nullptr) {
            return;
        }

        uint32_t index = INVALID_INDEX;
        for (size_t i = 0; i < group->setResources.size(); ++i) {
            if (group->setResources[i].binding == binding) {
                index = static_cast<uint32_t>(i);
                break;
            }
        }
        if (index == INVALID_INDEX) {
            return;
        }
        auto *smp = static_cast<D3D12Sampler *>(sampler);
        if (smp == nullptr) {
            return;
        }

        group->mDirty = true;
        const uint32_t slot   = group->allocation.samplerFirst + group->samplerOffsets[index] + arrayElement;
        const auto     handle = allocator->GetSamplerCpuHandle(slot);
        group->device->GetNativeHandle()->CreateSampler(&smp->GetNativeDesc(), handle);
    }

} // namespace sky::aurora
