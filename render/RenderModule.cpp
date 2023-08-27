//
// Created by Zach Lee on 2023/2/20.
//

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/application/SettingRegistry.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>

#include <render/adaptor/assets/Material.h>
#include <render/adaptor/assets/Shader.h>
#include <render/adaptor/assets/Technique.h>
#include <render/adaptor/assets/Mesh.h>
#include <render/adaptor/assets/Image.h>
#include <render/adaptor/assets/RenderPrefab.h>

#include <render/adaptor/components/CameraComponent.h>
#include <render/adaptor/components/LightComponent.h>
#include <render/adaptor/components/MeshRenderer.h>

#include <render/RHI.h>
#include <render/Renderer.h>

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
        MeshRenderer::Reflect();
        CameraComponent::Reflect();
    }

    class RenderModule : public IModule {
    public:
        RenderModule() = default;
        ~RenderModule() override = default;

        bool Init() override;
        void Start() override;
        void Stop() override;
        void Tick(float delta) override;
        void Shutdown() override;
    };

    bool RenderModule::Init()
    {
        auto *serializationContext = SerializationContext::Get();
        ReflectRenderAsset(serializationContext);
        ReflectRHI(serializationContext);

        RegisterComponents();

        return true;
    }

    void RenderModule::Start()
    {
        auto *iSys = Interface<ISystemNotify>::Get();
        auto &reg = iSys->GetApi()->GetSettings();

        rhi::Instance::Descriptor rhiDesc = {};
        rhiDesc.engineName = "SkyEngine";
        rhiDesc.appName = "";
        rhiDesc.api = rhi::GetApiByString(reg.VisitString("rhi"));

        // init rhi
        RHI::Get()->InitInstance(rhiDesc);
        RHI::Get()->InitDevice({});

        // init renderer
        Renderer::Get()->Init();
    }

    void RenderModule::Stop()
    {
    }

    void RenderModule::Shutdown()
    {
        Renderer::Destroy();
        RHI::Destroy();
    }

    void RenderModule::Tick(float delta)
    {
        Renderer::Get()->Tick(delta);
    }
}
REGISTER_MODULE(sky::RenderModule)