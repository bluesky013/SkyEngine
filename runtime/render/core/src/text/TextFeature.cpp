//
// Created by blues on 2024/9/13.
//

#include <render/text/TextFeature.h>
#include <render/text/TextRegistry.h>
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
        batchLayout = technique->RequestProgram()->RequestLayout(BATCH_SET);
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

    Text* TextFeatureProcessor::CreateText(const FontPtr &font)
    {
        auto *text = TextRegistry::Get()->CreateText(font);
        return texts.emplace_back(text).get();
    }

    void TextFeatureProcessor::RemoveText(Text *text)
    {
        texts.remove_if([text](const auto &val) {
            return text == val.get();
        });
    }
} // namespace sky