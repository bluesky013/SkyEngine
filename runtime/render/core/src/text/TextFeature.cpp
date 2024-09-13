//
// Created by blues on 2024/9/13.
//

#include <render/text/TextFeature.h>
#include <render/Renderer.h>
#include <render/RHI.h>

namespace sky {
    static constexpr uint32_t MAX_SET_PER_POOL = 16;
    static const std::vector<rhi::DescriptorSetPool::PoolSize> SIZES = {
        {rhi::DescriptorType::SAMPLED_IMAGE, MAX_SET_PER_POOL},
        {rhi::DescriptorType::SAMPLER, MAX_SET_PER_POOL},
    };

    void TextFeature::SetTechnique(const RDGfxTechPtr &tech)
    {
        technique = tech;
        localLayout = technique->RequestProgram()->RequestLayout(1);
    }

    void TextFeature::Init()
    {
        Renderer::Get()->RegisterRenderFeature<TextFeatureProcessor>();
        auto *device = RHI::Get()->GetDevice();

        {
            rhi::DescriptorSetPool::Descriptor poolDesc = {};

            poolDesc.maxSets   = MAX_SET_PER_POOL;
            poolDesc.sizeCount = static_cast<uint32_t>(SIZES.size());
            poolDesc.sizeData  = SIZES.data();
            pool               = device->CreateDescriptorSetPool(poolDesc);
        }
    }

    void TextFeatureProcessor::Tick(float time)
    {

    }

    void TextFeatureProcessor::Render(rdg::RenderGraph &rdg)
    {

    }
} // namespace sky