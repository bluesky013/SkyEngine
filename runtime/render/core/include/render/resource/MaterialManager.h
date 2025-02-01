//
// Created by blues on 2025/1/11.
//

#pragma once

#include <rhi/DescriptorSetPool.h>

namespace sky {

    class MaterialManager {
    public:
        MaterialManager() = default;
        ~MaterialManager() = default;

        void Init();
        const rhi::DescriptorSetPoolPtr &GetPool() const { return matSetPool; }

    private:
        rhi::DescriptorSetPoolPtr matSetPool;
    };

} // namespace sky
