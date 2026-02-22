//
// Created by Zach Lee on 2022/11/13.
//

#pragma once

#include <rhi/Core.h>
#include <rhi/DescriptorSet.h>

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
            uint32_t maxSets = 0;
            uint32_t sizeCount = 0;
            const PoolSize *sizeData = nullptr;
        };

        virtual DescriptorSetPtr Allocate(const rhi::DescriptorSet::Descriptor &desc) = 0;
    };
    using DescriptorSetPoolPtr = std::shared_ptr<DescriptorSetPool>;
} // namespace sky::rhi
