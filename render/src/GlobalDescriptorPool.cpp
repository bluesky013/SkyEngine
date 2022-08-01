//
// Created by Zach Lee on 2022/8/1.
//

#include <render/GlobalDescriptorPool.h>

namespace sky {

    RDDescriptorPoolPtr GlobalDescriptorPool::GetPool(drv::DescriptorSetLayoutPtr layout)
    {
        auto hash = layout->GetHash();
        auto iter = pools.find(hash);
        if (iter != pools.end()) {
            return iter->second;
        }

        auto pool = DescriptorPool::CreatePool(layout, DescriptorPool::Descriptor{});
        pools.emplace(hash, pool);
        return pool;
    }

}