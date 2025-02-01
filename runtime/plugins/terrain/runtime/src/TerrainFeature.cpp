//
// Created by blues on 2024/12/12.
//


#include <terrain/TerrainFeature.h>
#include <render/Renderer.h>
#include <render/RHI.h>

namespace sky {
    static constexpr uint32_t MAX_SET_PER_POOL = 512;
    static const std::vector<rhi::DescriptorSetPool::PoolSize> SIZES = {
            {rhi::DescriptorType::SAMPLED_IMAGE, MAX_SET_PER_POOL},
            {rhi::DescriptorType::SAMPLER, MAX_SET_PER_POOL},
    };

    static const std::vector<rhi::DescriptorSetLayout::SetBinding> BINDINGS = {
            {rhi::DescriptorType::SAMPLED_IMAGE, 1, 0, rhi::ShaderStageFlagBit::VS, "HeightMap"},
            {rhi::DescriptorType::SAMPLER, 1, 1, rhi::ShaderStageFlagBit::VS, "HeightMapSampler"},
    };


    void TerrainFeature::Init()
    {
        auto *device = RHI::Get()->GetDevice();

        localLayout = new ResourceGroupLayout();
        localLayout->SetRHILayout(device->CreateDescriptorSetLayout({BINDINGS}));
        localLayout->AddNameHandler(Name("HeightMap"), {0, 0});
        localLayout->AddNameHandler(Name("HeightMapSampler"), {1, 0});

        {
            rhi::DescriptorSetPool::Descriptor poolDesc = {};

            poolDesc.maxSets   = MAX_SET_PER_POOL;
            poolDesc.sizeCount = static_cast<uint32_t>(SIZES.size());
            poolDesc.sizeData  = SIZES.data();
            pool               = device->CreateDescriptorSetPool(poolDesc);
        }
    }

    RDResourceGroupPtr TerrainFeature::RequestResourceGroup()
    {
        auto *rsg = new ResourceGroup();
        rsg->Init(localLayout, *pool);
        return rsg;
    }

} // namespace sky