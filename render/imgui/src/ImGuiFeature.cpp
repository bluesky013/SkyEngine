//
// Created by blues on 2023/9/20.
//

#include <imgui/ImGuiFeature.h>
#include <render/RHI.h>
#include <render/Renderer.h>
#include <imgui/ImGuiFeatureProcessor.h>

namespace sky {

    static constexpr uint32_t MAX_SET_PER_POOL = 1;
    static std::vector<rhi::DescriptorSetPool::PoolSize> SIZES = {
            {rhi::DescriptorType::UNIFORM_BUFFER,         MAX_SET_PER_POOL},
            {rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC, MAX_SET_PER_POOL},
            {rhi::DescriptorType::SAMPLED_IMAGE,          MAX_SET_PER_POOL},
            {rhi::DescriptorType::SAMPLER,                MAX_SET_PER_POOL}
    };


    void ImGuiFeature::Init(const RDGfxTechPtr &tech)
    {
        instance.technique = tech;
        resLayout = tech->RequestProgram()->RequestLayout(BATCH_SET);

        auto *device = RHI::Get()->GetDevice();
        {
            rhi::DescriptorSetPool::Descriptor poolDesc = {};

            poolDesc.maxSets   = MAX_SET_PER_POOL;
            poolDesc.sizeCount = static_cast<uint32_t>(SIZES.size());
            poolDesc.sizeData  = SIZES.data();

            pool = device->CreateDescriptorSetPool(poolDesc);
        }

        Renderer::Get()->RegisterRenderFeature<ImGuiFeatureProcessor>();
    }

    RDResourceGroupPtr ImGuiFeature::RequestResourceGroup()
    {
        auto rsg = std::make_shared<ResourceGroup>();
        rsg->Init(resLayout, *pool);
        return rsg;
    }
} // namespace sky
