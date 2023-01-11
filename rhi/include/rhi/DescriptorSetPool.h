//
// Created by Zach Lee on 2022/11/13.
//

#pragma once

#include <rhi/Core.h>

namespace sky::rhi {

    class DescriptorSetPool {
    public:
        DescriptorSetPool() = default;
        ~DescriptorSetPool() = default;

        struct PoolSize {
            DescriptorType type = DescriptorType::SAMPLER;
            uint32_t count      = 0;
        };

        struct Descriptor {
            uint32_t              maxSets = 0;
            std::vector<PoolSize> sizes;
        };

    protected:
    };

}