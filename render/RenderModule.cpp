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

#include <imgui/ImGuiFeature.h>
#include <cxxopts.hpp>

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

        bool Init(const StartArguments &args) override;
        void Tick(float delta) override;
        void Shutdown() override;
        void Start() override;
    private:
        void ProcessArgs(const StartArguments &args);

        rhi::API api = rhi::API::DEFAULT;
    };

    void RenderModule::ProcessArgs(const StartArguments &args)
    {
        cxxopts::Options options("SkyEngine Render Module", "SkyEngine Render Module");
        options.allow_unrecognised_options();

        options.add_options()
            ("p,project", "Project Directory", cxxopts::value<std::string>());

        auto result = options.parse(static_cast<int32_t>(args.args.size()), args.args.data());

        if (result.count("rhi") != 0u) {
            api = rhi::GetApiByString(result["rhi"].as<std::string>());
        }
    }

    bool RenderModule::Init(const StartArguments &args)
    {
        auto *serializationContext = SerializationContext::Get();
        ReflectRenderAsset(serializationContext);
        ReflectRHI(serializationContext);

        RegisterComponents();

        ProcessArgs(args);

        rhi::Instance::Descriptor rhiDesc = {};
        rhiDesc.engineName = "SkyEngine";
        rhiDesc.appName = "";
#if _DEBUG
        rhiDesc.enableDebugLayer = true;
#else
        rhiDesc.enableDebugLayer = false;
#endif
        rhiDesc.api = api;

        // init rhi
        RHI::Get()->InitInstance(rhiDesc);
        RHI::Get()->InitDevice({});

        // init renderer
        Renderer::Get()->Init();
        return true;
    }

    void RenderModule::Start()
    {
        // init assets
        auto *am = AssetManager::Get();
        auto vtxLibAsset = AssetManager::Get()->LoadAsset<VertexDescLibrary>("vertex/vertex_library.vtxlib", false);
        Renderer::Get()->SetVertexDescLibrary(CreateVertexDescLibrary(vtxLibAsset->Data()));

//        ImGuiFeature::Get()->Init(AssetManager::Get()->LoadAsset<Technique>("techniques/gui.tech")->CreateInstanceAs<GraphicsTechnique>());
//        GeometryFeature::Get()->Init(AssetManager::Get()->LoadAsset<Technique>("techniques/geometry.tech")->CreateInstanceAs<GraphicsTechnique>());

//        MeshFeature::Get()->Init();
//        ParticleFeature::Get()->Init();
    }

    void RenderModule::Shutdown()
    {
        GeometryFeature::Destroy();
        MeshFeature::Destroy();
        ImGuiFeature::Destroy();
        ParticleFeature::Destroy();

        Renderer::Destroy();
        RHI::Destroy();
    }

    void RenderModule::Tick(float delta)
    {
        Renderer::Get()->Tick(delta);
    }
}
REGISTER_MODULE(sky::RenderModule)
