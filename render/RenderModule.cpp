//
// Created by Zach Lee on 2023/2/20.
//

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/application/SettingRegistry.h>

#include <render/adaptor/components/CameraComponent.h>
#include <render/adaptor/components/LightComponent.h>
#include <render/adaptor/components/MeshRenderer.h>
#include <render/adaptor/Reflection.h>

#include <render/geometry/GeometryFeature.h>
#include <render/particle/ParticleFeature.h>
#include <render/mesh/MeshFeature.h>

#include <render/RHI.h>
#include <render/Renderer.h>
#include <render/adaptor/assets/VertexDescLibraryAsset.h>
#include <render/resource/Technique.h>

namespace sky {

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

        auto *iSys = Interface<ISystemNotify>::Get();
        auto &reg = iSys->GetApi()->GetSettings();

        rhi::Instance::Descriptor rhiDesc = {};
        rhiDesc.engineName = "SkyEngine";
        rhiDesc.appName = "";
#if _DEBUG
        rhiDesc.enableDebugLayer = true;
#else
        rhiDesc.enableDebugLayer = false;
#endif
        rhiDesc.api = rhi::GetApiByString(reg.VisitString("rhi"));

        // init rhi
        RHI::Get()->InitInstance(rhiDesc);
        RHI::Get()->InitDevice({});

        // init renderer
        Renderer::Get()->Init();
        return true;
    }

    void RenderModule::Start()
    {
        auto vtxLibAsset = AssetManager::Get()->LoadAsset<VertexDescLibrary>("vertex_library.vtxlib");
        Renderer::Get()->SetVertexDescLibrary(CreateVertexDescLibrary(vtxLibAsset->Data()));

        GeometryFeature::Get()->Init(AssetManager::Get()->LoadAsset<Technique>("techniques/geometry.tech")->CreateInstanceAs<GraphicsTechnique>());
        MeshFeature::Get()->Init();
        ParticleFeature::Get()->Init();
    }

    void RenderModule::Stop()
    {
        GeometryFeature::Destroy();
        MeshFeature::Destroy();
        ParticleFeature::Destroy();
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
