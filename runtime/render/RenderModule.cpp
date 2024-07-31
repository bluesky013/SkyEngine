//
// Created by Zach Lee on 2023/2/20.
//

#include <framework/interface/IModule.h>
#include <framework/asset/AssetManager.h>
#include <framework/platform/PlatformBase.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/application/ModuleManager.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/ComponentFactory.h>

#include <render/adaptor/components/CameraComponent.h>
#include <render/adaptor/components/LightComponent.h>
#include <render/adaptor/components/StaticMeshComponent.h>
#include <render/adaptor/Reflection.h>
#include <render/adaptor/assets/TechniqueAsset.h>

#include <render/particle/ParticleFeature.h>
#include <render/mesh/MeshFeature.h>
#include <imgui/ImGuiFeature.h>

#include <render/RHI.h>
#include <render/Renderer.h>

#include <core/profile/Profiler.h>
#include <cxxopts.hpp>

namespace sky {

    static void RegisterComponents()
    {
        auto *context = SerializationContext::Get();
        LightComponent::Reflect(context);
        StaticMeshComponent::Reflect(context);
        CameraComponent::Reflect(context);

        static std::string GROUP = "Render";
        ComponentFactory::Get()->RegisterComponent<LightComponent>(GROUP);
        ComponentFactory::Get()->RegisterComponent<StaticMeshComponent>(GROUP);
        ComponentFactory::Get()->RegisterComponent<CameraComponent>(GROUP);
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
        void InitFeatures();

        rhi::API api = rhi::API::DEFAULT;
    };

    void RenderModule::ProcessArgs(const StartArguments &args)
    {
        cxxopts::Options options("SkyEngine Render Module", "SkyEngine Render Module");
        options.allow_unrecognised_options();

        options.add_options()
            ("p,project", "Project Directory", cxxopts::value<std::string>())
            ("r,rhi", "RHI Type", cxxopts::value<std::string>());

        if (!args.args.empty()) {
            auto result = options.parse(static_cast<int32_t>(args.args.size()), args.args.data());
            if (result.count("rhi") != 0u) {
                api = rhi::GetApiByString(result["rhi"].as<std::string>());
            }
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
#if _DEBUG && !__ANDROID__
        rhiDesc.enableDebugLayer = true;
#else
        rhiDesc.enableDebugLayer = false;
#endif
        rhiDesc.api = api;

        // init rhi
        RHI::Get()->InitInstance(rhiDesc);

        rhi::DeviceFeature feature = {};
        feature.multiView = true;
        RHI::Get()->InitDevice({feature});

        // init renderer
        Renderer::Get()->Init();
        Renderer::Get()->SetCacheFolder(Platform::Get()->GetInternalPath());
        return true;
    }

    void RenderModule::Start()
    {
        auto *mm = Interface<ISystemNotify>::Get()->GetApi()->GetModuleManager();
        auto shaderCompileFunc = mm->GetFunctionFomModule<ShaderCompileFunc>("ShaderCompiler", "CompileBinary");
        Renderer::Get()->SetShaderCompiler(shaderCompileFunc);

        InitFeatures();
    }

    void RenderModule::InitFeatures() // NOLINT
    {
        MeshFeature::Get()->Init();
        ImGuiFeature::Get()->Init();

        auto *am = AssetManager::Get();
        {
            auto guiAsset = am->LoadAssetFromPath<Technique>("techniques/gui.tech");
            guiAsset->BlockUntilLoaded();
            ImGuiFeature::Get()->SetTechnique(GreateGfxTechFromAsset(guiAsset));
        }

    }

    void RenderModule::Shutdown()
    {
        Renderer::Get()->StopRender();

        MeshFeature::Destroy();
        ImGuiFeature::Destroy();

        Renderer::Destroy();
        RHI::Destroy();
    }

    void RenderModule::Tick(float delta)
    {
        SKY_PROFILE_SCOPE;
        Renderer::Get()->Tick(delta);
    }
}
REGISTER_MODULE(sky::RenderModule)
