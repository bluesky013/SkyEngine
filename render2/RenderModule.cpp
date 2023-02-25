//
// Created by Zach Lee on 2023/2/20.
//

#include <framework/interface/IModule.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>

#include <render/assets/Material.h>
#include <render/assets/Shader.h>
#include <render/assets/Technique.h>

#include <rhi/Core.h>

namespace sky {

    static void ReflectRenderAsset(SerializationContext *context)
    {
        context->Register<ShaderVariantData>("ShaderVariantData")
            .BinLoad<&ShaderVariantData::Load>()
            .BinSave<&ShaderVariantData::Save>();

        context->Register<ShaderAssetData>("ShaderAssetData")
            .BinLoad<&ShaderAssetData::Load>()
            .BinSave<&ShaderAssetData::Save>();

        context->Register<MaterialAssetData>("MaterialAssetData")
            .BinLoad<&MaterialAssetData::Load>()
            .BinSave<&MaterialAssetData::Save>();

        context->Register<TechniqueAssetData>("TechniqueAssetData")
            .BinLoad<&TechniqueAssetData::Load>()
            .BinSave<&TechniqueAssetData::Save>();

        auto *am = AssetManager::Get();
        am->RegisterAssetHandler<Shader>();
        am->RegisterAssetHandler<ShaderVariant>();
        am->RegisterAssetHandler<Material>();
        am->RegisterAssetHandler<Technique>();
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
    }
}
REGISTER_MODULE(sky::RenderModule)