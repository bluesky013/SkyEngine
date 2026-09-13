//
// Created on 2026/09/13.
//

#include <D3D12ResourceGroup.h>
#include <D3D12Buffer.h>
#include <D3D12Device.h>
#include <D3D12Image.h>
#include <D3D12Sampler.h>
#include <D3D12ShaderFunction.h>
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
        if (desc.shader == nullptr) {
            LOG_E(TAG, "ResourceGroup requires a non-null shader");
            return false;
        }
        shader = static_cast<D3D12Shader *>(desc.shader);

        // snapshot this set's resources from reflection and compute descriptor layout
        setResources.clear();
        for (const auto &res : shader->GetReflection().resources) {
            if (res.set == desc.set) {
                setResources.push_back(res);
            }
        }
        if (setResources.empty()) {
            LOG_E(TAG, "shader has no descriptor set %u", desc.set);
            return false;
        }

        cbvSrvUavOffsets.resize(setResources.size());
        samplerOffsets.resize(setResources.size());

        for (size_t i = 0; i < setResources.size(); ++i) {
            const auto &res      = setResources[i];
            cbvSrvUavOffsets[i]  = cbvSrvUavCount;
            samplerOffsets[i]    = samplerCount;

            if (res.type == ShaderResourceType::SAMPLER) {
                samplerCount += res.count;
            } else if (res.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC ||
                       res.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC) {
                // dynamic binding: no descriptor; rebound per-draw via root CBV/UAV
                DynamicBinding db{};
                db.binding = res.binding;
                db.type    = res.type;
                dynamicBindings.push_back(db);
            } else {
                cbvSrvUavCount += res.count;
            }
        }

        auto *allocator = device->GetDescriptorAllocator();
        if (allocator == nullptr) {
            LOG_E(TAG, "descriptor allocator is not initialized");
            return false;
        }
        if (!allocator->Allocate(cbvSrvUavCount, samplerCount, allocation)) {
            LOG_E(TAG, "descriptor heap allocation failed");
            return false;
        }
        return true;
    }

    void D3D12ResourceGroup::Update(const std::vector<ResourceUpdateInfo> &writes)
    {
        if (writes.empty()) {
            return;
        }

        auto *allocator = device->GetDescriptorAllocator();
        if (allocator == nullptr) {
            return;
        }

        auto findIndex = [&](uint32_t binding) -> uint32_t {
            for (size_t i = 0; i < setResources.size(); ++i) {
                if (setResources[i].binding == binding) {
                    return static_cast<uint32_t>(i);
                }
            }
            return INVALID_INDEX;
        };

        for (const auto &w : writes) {
            const uint32_t index = findIndex(w.binding);
            if (index == INVALID_INDEX) {
                continue;
            }
            const auto &res = setResources[index];

            switch (w.kind) {
            case ResourceWriteKind::BUFFER: {
                auto *buf = static_cast<D3D12Buffer *>(w.buffer);
                if (buf == nullptr) {
                    break;
                }
                if (res.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC ||
                    res.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC) {
                    // record the buffer; address is rebound at bind time with offset
                    for (auto &db : dynamicBindings) {
                        if (db.binding == res.binding) {
                            db.buffer     = buf;
                            db.baseOffset = w.bufferOffset;
                            db.range      = w.bufferRange;
                            break;
                        }
                    }
                    break;
                }
                const uint32_t slot   = allocation.cbvSrvUavFirst + cbvSrvUavOffsets[index] + w.arrayElement;
                const auto     handle = allocator->GetCbvSrvUavCpuHandle(slot);
                if (res.type == ShaderResourceType::STORAGE_BUFFER) {
                    buf->CreateUAV(handle);
                } else if (res.type == ShaderResourceType::UNIFORM_BUFFER) {
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
                const uint32_t slot   = allocation.cbvSrvUavFirst + cbvSrvUavOffsets[index] + w.arrayElement;
                const auto     handle = allocator->GetCbvSrvUavCpuHandle(slot);
                if (res.type == ShaderResourceType::STORAGE_IMAGE) {
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
                const uint32_t slot   = allocation.samplerFirst + samplerOffsets[index] + w.arrayElement;
                const auto     handle = allocator->GetSamplerCpuHandle(slot);
                device->GetNativeHandle()->CreateSampler(&smp->GetNativeDesc(), handle);
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
