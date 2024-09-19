//
// Created by Zach Lee on 2023/9/3.
//

#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>
#include <render/adaptor/Reflection.h>

#include <render/adaptor/assets/ImageAsset.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/adaptor/assets/RenderPrefab.h>
#include <render/adaptor/assets/TechniqueAsset.h>
#include <render/adaptor/assets/BufferAsset.h>
#include <render/adaptor/assets/ShaderAsset.h>
#include <render/adaptor/assets/AnimationAsset.h>
#include <render/adaptor/assets/SkeletonAsset.h>

namespace sky {

    void ReflectRenderAsset(SerializationContext *context)
    {
        context->Register<MaterialTexture>("MaterialTexture")
            .Member<&MaterialTexture::texIndex>("texIndex");

        context->Register<MaterialAssetData>("MaterialAssetData")
            .BinLoad<&MaterialAssetData::LoadBin>()
            .BinSave<&MaterialAssetData::SaveBin>();

        context->Register<MaterialInstanceData>("MaterialInstanceData")
            .BinLoad<&MaterialInstanceData::LoadBin>()
            .BinSave<&MaterialInstanceData::SaveBin>();

        context->Register<TechniqueAssetData>("TechniqueAssetData")
            .BinLoad<&TechniqueAssetData::Load>()
            .BinSave<&TechniqueAssetData::Save>();

        context->Register<ImageAssetData>("ImageAssetData")
            .BinLoad<&ImageAssetData::Load>()
            .BinSave<&ImageAssetData::Save>();

        context->Register<MeshAssetData>("MeshAssetData")
            .BinLoad<&MeshAssetData::Load>()
            .BinSave<&MeshAssetData::Save>();

        context->Register<RenderPrefabAssetData>("RenderPrefabAssetData")
            .BinLoad<&RenderPrefabAssetData::LoadBin>()
            .BinSave<&RenderPrefabAssetData::SaveBin>()
            .JsonLoad<&RenderPrefabAssetData::LoadJson>()
            .JsonSave<&RenderPrefabAssetData::SaveJson>();

        context->Register<BufferAssetData>("BufferAssetData")
            .BinLoad<&BufferAssetData::Load>()
            .BinSave<&BufferAssetData::Save>();

        context->Register<ShaderAssetData>("ShaderAssetData")
            .BinLoad<&ShaderAssetData::Load>()
            .BinSave<&ShaderAssetData::Save>();

        context->Register<AnimationAssetData>("AnimationAssetData")
            .BinLoad<&AnimationAssetData::Load>()
            .BinSave<&AnimationAssetData::Save>();

        context->Register<SkeletonAssetData>("SkeletonData")
                .BinLoad<&SkeletonAssetData::Load>()
                .BinSave<&SkeletonAssetData::Save>();

        auto *am = AssetManager::Get();
        am->RegisterAssetHandler<Material>();
        am->RegisterAssetHandler<MaterialInstance>();
        am->RegisterAssetHandler<Technique>();
        am->RegisterAssetHandler<Mesh>();
        am->RegisterAssetHandler<Texture>();
        am->RegisterAssetHandler<RenderPrefab>();
        am->RegisterAssetHandler<Buffer>();
        am->RegisterAssetHandler<ShaderCollection>();
        am->RegisterAssetHandler<Animation>();
        am->RegisterAssetHandler<Skeleton>();
    }

    void ReflectRHI(SerializationContext *context)
    {
        context->Register<rhi::StencilState>("RHI_StencilState")
            .Member<&rhi::StencilState::failOp>("failOp")
            .Member<&rhi::StencilState::passOp>("passOp")
            .Member<&rhi::StencilState::depthFailOp>("depthFailOp")
            .Member<&rhi::StencilState::compareOp>("compareOp")
            .Member<&rhi::StencilState::compareMask>("compareMask")
            .Member<&rhi::StencilState::writeMask>("writeMask")
            .Member<&rhi::StencilState::reference>("reference");

        context->Register<rhi::DepthStencil>("RHI_DepthStencil")
            .Member<&rhi::DepthStencil::depthTest>("depthTest")
            .Member<&rhi::DepthStencil::depthWrite>("depthWrite")
            .Member<&rhi::DepthStencil::stencilTest>("stencilTest")
            .Member<&rhi::DepthStencil::compareOp>("compareOp")
            .Member<&rhi::DepthStencil::minDepth>("minDepth")
            .Member<&rhi::DepthStencil::maxDepth>("maxDepth")
            .Member<&rhi::DepthStencil::front>("front")
            .Member<&rhi::DepthStencil::back>("back");

        context->Register<rhi::RasterState>("RHI_RasterState")
            .Member<&rhi::RasterState::depthClampEnable       >("depthClampEnable")
            .Member<&rhi::RasterState::rasterizerDiscardEnable>("rasterizerDiscardEnable")
            .Member<&rhi::RasterState::depthBiasEnable        >("depthBiasEnable")
            .Member<&rhi::RasterState::depthBiasConstantFactor>("depthBiasConstantFactor")
            .Member<&rhi::RasterState::depthBiasClamp         >("depthBiasClamp")
            .Member<&rhi::RasterState::depthBiasSlopeFactor   >("depthBiasSlopeFactor")
            .Member<&rhi::RasterState::lineWidth              >("lineWidth")
            .Member<&rhi::RasterState::cullMode               >("cullMode")
            .Member<&rhi::RasterState::frontFace              >("frontFace")
            .Member<&rhi::RasterState::polygonMode            >("polygonMode");
    }
} // namespace sky
