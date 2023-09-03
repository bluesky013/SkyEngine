//
// Created by Zach Lee on 2023/9/3.
//

#include <render/adaptor/Reflection.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>

#include <render/adaptor/assets/ImageAsset.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/adaptor/assets/RenderPrefab.h>
#include <render/adaptor/assets/ShaderAsset.h>
#include <render/adaptor/assets/TechniqueAsset.h>

namespace sky {

    void ReflectRenderAsset(SerializationContext *context)
    {
        context->Register<MaterialTexture>("MaterialTexture")
            .Member<&MaterialTexture::texIndex>("texIndex");

        context->Register<ShaderVariantData>("ShaderVariantData")
            .BinLoad<&ShaderVariantData::Load>()
            .BinSave<&ShaderVariantData::Save>();

        context->Register<ShaderAssetData>("ShaderAssetData")
            .BinLoad<&ShaderAssetData::Load>()
            .BinSave<&ShaderAssetData::Save>();

        context->Register<MaterialAssetData>("MaterialAssetData")
            .BinLoad<&MaterialAssetData::LoadBin>()
            .BinSave<&MaterialAssetData::SaveBin>()
            .JsonLoad<&MaterialAssetData::LoadJson>()
            .JsonSave<&MaterialAssetData::SaveJson>();

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
            .BinLoad<&RenderPrefabAssetData::Load>()
            .BinSave<&RenderPrefabAssetData::Save>();

        auto *am = AssetManager::Get();
        am->RegisterAssetHandler<Shader>();
        am->RegisterAssetHandler<ShaderVariant>();
        am->RegisterAssetHandler<Material>();
        am->RegisterAssetHandler<Technique>();
        am->RegisterAssetHandler<Mesh>();
        am->RegisterAssetHandler<Texture>();
        am->RegisterAssetHandler<RenderPrefab>();
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
    }
} // namespace sky