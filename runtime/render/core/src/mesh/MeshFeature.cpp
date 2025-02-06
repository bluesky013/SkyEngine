//
// Created by Zach Lee on 2023/9/9.
//

#include <render/mesh/MeshFeature.h>
#include <render/skeleton/SkeletonMeshRenderer.h>
#include <render/Renderer.h>
#include <render/RHI.h>

#include <render/mesh/MeshFeatureProcessor.h>

namespace sky {
    static constexpr uint32_t MAX_SET_PER_POOL = 128;
    static const std::vector<rhi::DescriptorSetPool::PoolSize> SIZES = {
        {rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC, MAX_SET_PER_POOL}
    };

    static const std::vector<rhi::DescriptorSetLayout::SetBinding> BINDINGS = {
        {rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC, 1, 0, rhi::ShaderStageFlagBit::VS, "Local"},
    };

    static const std::vector<rhi::DescriptorSetLayout::SetBinding> SKINNED_BINDINGS = {
        {rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC, 1, 0, rhi::ShaderStageFlagBit::VS, "Local"},
        {rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC, 1, 1, rhi::ShaderStageFlagBit::VS, "Skin"},
    };

    // mesh shading
    static const std::vector<rhi::DescriptorSetPool::PoolSize> MESH_SIZES = {
        {rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC, 2 * MAX_SET_PER_POOL},
        {rhi::DescriptorType::STORAGE_BUFFER, 6 * MAX_SET_PER_POOL}
    };

    static const std::vector<rhi::DescriptorSetLayout::SetBinding> MESH_BINDINGS = {
        {rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC, 1, 0, rhi::ShaderStageFlagBit::TAS | rhi::ShaderStageFlagBit::MS, "Local"},
        {rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC, 1, 2, rhi::ShaderStageFlagBit::TAS | rhi::ShaderStageFlagBit::MS, "MeshletInfo"},
        {rhi::DescriptorType::STORAGE_BUFFER, 1, 3, rhi::ShaderStageFlagBit::MS, "PositionBuf"},
        {rhi::DescriptorType::STORAGE_BUFFER, 1, 4, rhi::ShaderStageFlagBit::MS, "ExtBuf"},
        {rhi::DescriptorType::STORAGE_BUFFER, 1, 5, rhi::ShaderStageFlagBit::MS, "VertexIndices"},
        {rhi::DescriptorType::STORAGE_BUFFER, 1, 6, rhi::ShaderStageFlagBit::MS, "MeshletTriangles"},
        {rhi::DescriptorType::STORAGE_BUFFER, 1, 7, rhi::ShaderStageFlagBit::TAS | rhi::ShaderStageFlagBit::MS, "Meshlets"},
        //        {rhi::DescriptorType::STORAGE_BUFFER, 1, 7, rhi::ShaderStageFlagBit::MS, "MeshletCullData"},
    };

    void MeshFeature::Init()
    {
        Renderer::Get()->RegisterRenderFeature<MeshFeatureProcessor>();
        auto *device = RHI::Get()->GetDevice();

        localLayout = new ResourceGroupLayout();
        localLayout->SetRHILayout(device->CreateDescriptorSetLayout({BINDINGS}));
        localLayout->AddNameHandler(Name("Local"), {0, sizeof(InstanceLocal)});

        skinnedLayout = new ResourceGroupLayout();
        skinnedLayout->SetRHILayout(device->CreateDescriptorSetLayout({SKINNED_BINDINGS}));
        skinnedLayout->AddNameHandler(Name("Local"), {0, sizeof(InstanceLocal)});
        skinnedLayout->AddNameHandler(Name("skinData"), {1, MAX_BONE_NUM * sizeof(Matrix4)});

        meshLayout = new ResourceGroupLayout();
        meshLayout->SetRHILayout(device->CreateDescriptorSetLayout({MESH_BINDINGS}));
        meshLayout->AddNameHandler(Name("Local"), {0, sizeof(InstanceLocal)});
        meshLayout->AddNameHandler(Name("MeshletInfo"), {2, sizeof(MeshletInfo)});
        meshLayout->AddNameHandler(Name("PositionBuf"), {3, 0});
        meshLayout->AddNameHandler(Name("ExtBuf"), {4, 0});
        meshLayout->AddNameHandler(Name("VertexIndices"), {5, 0});
        meshLayout->AddNameHandler(Name("MeshletTriangles"), {6, 0});
        meshLayout->AddNameHandler(Name("Meshlets"), {7, 0});

        {
            rhi::DescriptorSetPool::Descriptor poolDesc = {};

            poolDesc.maxSets   = MAX_SET_PER_POOL;
            poolDesc.sizeCount = static_cast<uint32_t>(SIZES.size());
            poolDesc.sizeData  = SIZES.data();
            pool               = device->CreateDescriptorSetPool(poolDesc);
        }

        {
            rhi::DescriptorSetPool::Descriptor poolDesc = {};

            poolDesc.maxSets   = MAX_SET_PER_POOL;
            poolDesc.sizeCount = static_cast<uint32_t>(MESH_SIZES.size());
            poolDesc.sizeData  = MESH_SIZES.data();
            meshPool           = device->CreateDescriptorSetPool(poolDesc);
        }
    }

    RDResourceGroupPtr MeshFeature::RequestResourceGroup()
    {
        auto *rsg = new ResourceGroup();
        rsg->Init(localLayout, *pool);
        return rsg;
    }

    RDResourceGroupPtr MeshFeature::RequestMeshResourceGroup()
    {
        auto *rsg = new ResourceGroup();
        rsg->Init(meshLayout, *meshPool);
        return rsg;
    }

    RDResourceGroupPtr MeshFeature::RequestSkinnedResourceGroup()
    {
        auto *rsg = new ResourceGroup();
        rsg->Init(skinnedLayout, *pool);
        return rsg;
    }

} // namespace sky
