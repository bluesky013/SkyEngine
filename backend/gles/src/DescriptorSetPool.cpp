//
// Created by Zach Lee on 2023/5/13.
//

#include <gles/DescriptorSetPool.h>
#include <gles/DescriptorSet.h>
#include <gles/Device.h>

namespace sky::gles {

    rhi::DescriptorSetPtr DescriptorSetPool::Allocate(const rhi::DescriptorSet::Descriptor &desc)
    {
        auto res = std::make_shared<DescriptorSet>(device);
        if (!res->Init(desc)) {
            return {};
        }
        return res;
    }

    bool DescriptorSetPool::Init(const Descriptor &desc)
    {
        return true;
    }

} // namespace sky::gles