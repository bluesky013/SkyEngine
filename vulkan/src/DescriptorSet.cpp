//
// Created by Zach Lee on 2022/1/26.
//

#include <vulkan/DescriptorSet.h>
#include <vulkan/DescriptorSetPool.h>

namespace sky::drv {

    DescriptorSet::~DescriptorSet()
    {
        auto ptr = pool.lock();
        if (ptr) {
            ptr->Free(*this);
        }
    }

}