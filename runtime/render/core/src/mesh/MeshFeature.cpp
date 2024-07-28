//
// Created by Zach Lee on 2023/9/9.
//

#include <render/mesh/MeshFeature.h>
#include <render/Renderer.h>
#include <render/RHI.h>

#include <render/mesh/MeshFeatureProcessor.h>

namespace sky {
    static constexpr uint32_t MAX_SET_PER_POOL = 128;
    static const std::vector<rhi::DescriptorSetPool::PoolSize> SIZES = {
        {rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC, MAX_SET_PER_POOL}
    };

    static const std::vector<rhi::DescriptorSetLayout::SetBinding> BINDINGS = {
        {rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC, 1, 0, rhi::ShaderStageFlagBit::VS, "localData"},
    };

    void MeshFeature::Init()
    {
        Renderer::Get()->RegisterRenderFeature<MeshFeatureProcessor>();
        auto *device = RHI::Get()->GetDevice();

        localLayout = new ResourceGroupLayout();
        localLayout->SetRHILayout(device->CreateDescriptorSetLayout({BINDINGS}));
        localLayout->AddNameHandler("localData", {0, sizeof(InstanceLocal)});

        {
            rhi::DescriptorSetPool::Descriptor poolDesc = {};

            poolDesc.maxSets   = MAX_SET_PER_POOL;
            poolDesc.sizeCount = static_cast<uint32_t>(SIZES.size());
            poolDesc.sizeData  = SIZES.data();
            pool               = device->CreateDescriptorSetPool(poolDesc);
        }
    }

    RDResourceGroupPtr MeshFeature::RequestResourceGroup()
    {
        auto rsg = new ResourceGroup();
        rsg->Init(localLayout, *pool);
        return rsg;
    }

} // namespace sky
