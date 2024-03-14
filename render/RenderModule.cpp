//
// Created by Zach Lee on 2023/2/20.
//

#include <framework/interface/IModule.h>
#include <framework/asset/AssetManager.h>
#include <framework/platform/PlatformBase.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/application/ModuleManager.h>

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

#include <core/profile/Profiler.h>

#include <imgui/ImGuiFeature.h>

#include <shader/ShaderCompiler.h>

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

        // init assets
        auto *am = AssetManager::Get();

        // init shader compiler
#if SKY_EDITOR
        auto *compiler = ShaderCompiler::Get();
        for (const auto &path : am->GetSearchPathList()) {
            compiler->AddSearchPath(path.path);
        }
#endif

        auto vfAsset = AssetManager::Get()->LoadAsset<VertexDescLibrary>("vertex/vertex_library.vtxlib", false);
        if (vfAsset) {
            Renderer::Get()->SetVertexDescLibrary(CreateVertexDescLibrary(vfAsset->Data()));
        }

        ImGuiFeature::Get()->Init(AssetManager::Get()->LoadAsset<Technique>("techniques/gui.tech")->CreateInstanceAs<GraphicsTechnique>());
//        GeometryFeature::Get()->Init(AssetManager::Get()->LoadAsset<Technique>("techniques/geometry.tech")->CreateInstanceAs<GraphicsTechnique>());

        MeshFeature::Get()->Init();
//        ParticleFeature::Get()->Init();
    }

    void RenderModule::Shutdown()
    {
        GeometryFeature::Destroy();
        MeshFeature::Destroy();
        ParticleFeature::Destroy();

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
