//
// Created by Zach Lee on 2023/9/3.
//

#include <render/geometry/GeometryFeature.h>
#include <render/RHI.h>

namespace sky {

    static constexpr uint32_t MAX_SET_PER_POOL = 8;
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
