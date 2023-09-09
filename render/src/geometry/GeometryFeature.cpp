//
// Created by Zach Lee on 2023/9/3.
//

#include <render/geometry/GeometryFeature.h>
#include <render/RHI.h>

namespace sky {

    static constexpr uint32_t MAX_SET_PER_POOL = 32;
    static std::vector<rhi::DescriptorSetPool::PoolSize> SIZES = {
        {rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC, MAX_SET_PER_POOL}
    };

    void GeometryFeature::Init(const RDGfxTechPtr &tech)
    {
        technique = tech;
        auto rhiLayout = technique->RequestProgram()->GetPipelineLayout()->GetSetLayout(2);
        localLayout = std::make_shared<ResourceGroupLayout>();
        localLayout->SetRHILayout(rhiLayout);
        localLayout->AddNameHandler("ObjectInfo", 0);

        auto *device = RHI::Get()->GetDevice();
        {
            rhi::VertexInput::Descriptor viDesc = {};

            viDesc.attributes.reserve(2);
            viDesc.attributes.emplace_back(rhi::VertexAttributeDesc{0, 0, 0, rhi::Format::F_RGBA32});
            viDesc.attributes.emplace_back(rhi::VertexAttributeDesc{1, 0, 16, rhi::Format::F_RGBA32});

            viDesc.bindings.emplace_back(rhi::VertexBindingDesc{0, 32, rhi::VertexInputRate::PER_VERTEX});
            vertexDesc = device->CreateVertexInput(viDesc);
        }

        {
            rhi::DescriptorSetPool::Descriptor poolDesc = {};

            poolDesc.maxSets   = MAX_SET_PER_POOL;
            poolDesc.sizeCount = static_cast<uint32_t>(SIZES.size());
            poolDesc.sizeData  = SIZES.data();
            pool               = device->CreateDescriptorSetPool(poolDesc);
        }
    }

    RDResourceGroupPtr GeometryFeature::RequestResourceGroup()
    {
        auto rsg = std::make_shared<ResourceGroup>();
        rsg->Init(localLayout, *pool);
        return rsg;
    }

} // namespace sky
