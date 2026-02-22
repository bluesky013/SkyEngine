//
// Created by Zach Lee on 2025/7/20.
//

#include <mtl/DescriptorSetPool.h>
#include <mtl/DescriptorSet.h>

namespace sky::mtl {

    DescriptorSetPool::~DescriptorSetPool()
    {

    }

    bool DescriptorSetPool::Init(const Descriptor &)
    {
        return true;
    }

    rhi::DescriptorSetPtr DescriptorSetPool::Allocate(const rhi::DescriptorSet::Descriptor &desc)
    {
        auto res = std::make_shared<DescriptorSet>(device);
        if (res->Init(desc)) {
            return res;
        }
        return {};
    }

} // namespace sky::mtl
