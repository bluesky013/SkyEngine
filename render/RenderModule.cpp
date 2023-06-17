//
// Created by Zach Lee on 2023/2/20.
//

#include <framework/interface/IModule.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>

#include <render/assets/Material.h>
#include <render/assets/Shader.h>
#include <render/assets/Technique.h>
#include <render/assets/Mesh.h>
#include <render/assets/Image.h>
#include <render/assets/RenderPrefab.h>

#include <rhi/Core.h>

#include <engine/base/Component.h>
#include <render/adaptor/LightComponent.h>
#include <render/adaptor/MeshComponent.h>

namespace sky {

    static void ReflectRenderAsset(SerializationContext *context)
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
        am->RegisterAssetHandler<Image>();
        am->RegisterAssetHandler<RenderPrefab>();
    }

    static void ReflectRHI(SerializationContext *context)
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

    static void RegisterComponents()
    {
        LightComponent::Reflect();
        MeshComponent::Reflect();
    }

    class RenderModule : public IModule {
    public:
        RenderModule() = default;
        ~RenderModule() override = default;

        bool Init() override;
        void Start() override;
        void Stop() override {}
        void Tick(float delta) override {}
    };

    bool RenderModule::Init()
    {
        return true;
    }

    void RenderModule::Start()
    {
        auto *serializationContext = SerializationContext::Get();
        ReflectRenderAsset(serializationContext);
        ReflectRHI(serializationContext);
        RegisterComponents();
    }
}
REGISTER_MODULE(sky::RenderModule)