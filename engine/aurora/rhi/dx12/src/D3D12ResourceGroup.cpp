//
// Created on 2026/09/13.
//

#include <D3D12ResourceGroup.h>

#include <D3D12Buffer.h>
#include <D3D12DescriptorEncoder.h>
#include <D3D12Device.h>
#include <D3D12Image.h>
#include <D3D12Sampler.h>
#include <D3D12ShaderFunction.h>
#include <core/logger/Logger.h>

#include <algorithm>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {

    D3D12ResourceGroup::D3D12ResourceGroup(D3D12Device &dev) : device(&dev)
    {
    }

    const Device *D3D12ResourceGroup::GetDevice() const noexcept
    {
        return device;
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
        if (desc.shader == nullptr || desc.shader->GetDevice() != device) {
            LOG_E(TAG, "ResourceGroup requires a shader from this device");
            return false;
        }
        if (shader || !InitParameterLayout(desc))
            return false;
        shader = static_cast<D3D12Shader *>(desc.shader);

        // snapshot this set's resources from reflection and compute descriptor layout
        setResources.clear();
        for (const auto &res : shader->GetReflection().resources) {
            if (res.set == desc.set) {
                setResources.push_back(res);
            }
        }
        if (setResources.empty()) {
            return true;
        }

        cbvSrvUavOffsets.resize(setResources.size());
        samplerOffsets.resize(setResources.size());

        for (size_t i = 0; i < setResources.size(); ++i) {
            const auto &res     = setResources[i];
            cbvSrvUavOffsets[i] = cbvSrvUavCount;
            samplerOffsets[i]   = samplerCount;

            if (res.type == ShaderResourceType::SAMPLER) {
                if (!res.count || res.count > UINT32_MAX - samplerCount)
                    return false;
                samplerCount += res.count;
            } else if (res.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC || res.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC) {
                // Dynamic buffers bind through native root descriptors rather than table entries.
                DynamicBinding db{};
                db.binding  = res.binding;
                db.resource = res;
                dynamicBindings.push_back(db);
            } else {
                if (!res.count || res.count > UINT32_MAX - cbvSrvUavCount)
                    return false;
                cbvSrvUavCount += res.count;
            }
        }

        std::sort(dynamicBindings.begin(), dynamicBindings.end(), [](const auto &a, const auto &b) { return a.binding < b.binding; });
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

    std::unique_ptr<DescriptorEncoder> D3D12ResourceGroup::CreateEncoder()
    {
        if (IsParameterSnapshot())
            return {};
        return std::make_unique<D3D12DescriptorEncoder>(*this);
    }

    void D3D12ResourceGroup::EnsureFrameCopy()
    {
        auto *allocator = device->GetDescriptorAllocator();
        if (allocator == nullptr) {
            return;
        }
        if (mDirty || mCopiedFrame != allocator->GetCurrentFrame()) {
            allocator->CopyCbvSrvUav(allocation.cbvSrvUavFirst, allocation.cbvSrvUavCount);
            allocator->CopySampler(allocation.samplerFirst, allocation.samplerCount);
            mCopiedFrame = allocator->GetCurrentFrame();
            mDirty       = false;
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
