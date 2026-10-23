//
// Created on 2026/09/14.
//

#include <D3D12DescriptorBatch.h>

#include <D3D12DescriptorEncoder.h>
#include <D3D12Device.h>
#include <D3D12ResourceGroup.h>

namespace sky::aurora {

    D3D12DescriptorBatch::D3D12DescriptorBatch(D3D12Device &dev) : device(&dev)
    {
    }

    bool
    D3D12DescriptorBatch::WriteBuffer(ResourceGroup *group, uint32_t binding, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement)
    {
        if (failed || !group || group->GetDevice() != device || group->IsParameterSnapshot()) {
            failed = true;
            return false;
        }
        auto                  *d3dGroup = static_cast<D3D12ResourceGroup *>(group);
        D3D12DescriptorEncoder encoder(*d3dGroup);
        const bool             written = encoder.WriteBuffer(binding, buffer, offset, range, arrayElement);
        failed                         = !written || !encoder.End();
        return !failed;
    }

    bool D3D12DescriptorBatch::WriteImage(ResourceGroup *group, uint32_t binding, Image *image, ImageLayout layout, uint32_t arrayElement)
    {
        if (failed || !group || group->GetDevice() != device || group->IsParameterSnapshot()) {
            failed = true;
            return false;
        }
        auto                  *d3dGroup = static_cast<D3D12ResourceGroup *>(group);
        D3D12DescriptorEncoder encoder(*d3dGroup);
        const bool             written = encoder.WriteImage(binding, image, layout, arrayElement);
        failed                         = !written || !encoder.End();
        return !failed;
    }

    bool D3D12DescriptorBatch::WriteSampler(ResourceGroup *group, uint32_t binding, Sampler *sampler, uint32_t arrayElement)
    {
        if (failed || !group || group->GetDevice() != device || group->IsParameterSnapshot()) {
            failed = true;
            return false;
        }
        auto                  *d3dGroup = static_cast<D3D12ResourceGroup *>(group);
        D3D12DescriptorEncoder encoder(*d3dGroup);
        const bool             written = encoder.WriteSampler(binding, sampler, arrayElement);
        failed                         = !written || !encoder.End();
        return !failed;
    }

} // namespace sky::aurora
