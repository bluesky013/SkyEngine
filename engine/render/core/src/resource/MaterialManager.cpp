//
// Created by blues on 2025/1/11.
//

#include <render/resource/MaterialManager.h>
#include <render/RHI.h>

namespace sky {

    static constexpr uint32_t MAX_SET_PER_POOL = 8;
    static std::vector<rhi::DescriptorSetPool::PoolSize> SIZES = {
        {rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC, MAX_SET_PER_POOL},
        {rhi::DescriptorType::SAMPLED_IMAGE,          16 * MAX_SET_PER_POOL},
        {rhi::DescriptorType::SAMPLER,                16 * MAX_SET_PER_POOL},
    };

    void MaterialManager::Init()
    {
        rhi::DescriptorSetPool::Descriptor desc = {};
        desc.maxSets = MAX_SET_PER_POOL;
        desc.sizeCount = static_cast<uint32_t>(SIZES.size());
        desc.sizeData = SIZES.data();

        matSetPool = RHI::Get()->GetDevice()->CreateDescriptorSetPool(desc);
    }

} // namespace sky