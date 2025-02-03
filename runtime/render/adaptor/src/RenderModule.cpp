//
// Created by Zach Lee on 2023/2/20.
//

#include <render/adaptor/RenderModule.h>

#include <framework/asset/AssetManager.h>
#include <framework/platform/PlatformBase.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/application/ModuleManager.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/World.h>
#include <framework/world/ComponentFactory.h>

#include <render/adaptor/components/CameraComponent.h>
#include <render/adaptor/components/LightComponent.h>
#include <render/adaptor/components/StaticMeshComponent.h>
#include <render/adaptor/components/SkeletonMeshComponent.h>
#include <render/adaptor/components/SkyBoxComponent.h>
#include <render/adaptor/Reflection.h>
#include <render/adaptor/assets/TechniqueAsset.h>

#include <render/light/LightFeature.h>
#include <render/mesh/MeshFeature.h>
#include <render/text/TextFeature.h>
#include <imgui/ImGuiFeature.h>

#include <render/RHI.h>
#include <render/Renderer.h>

#include <core/profile/Profiler.h>
#include <cxxopts.hpp>

namespace sky {

    static void RegisterComponents()
    {
        auto *context = SerializationContext::Get();
        MainDirectLightComponent::Reflect(context);
        StaticMeshComponent::Reflect(context);
        CameraComponent::Reflect(context);
        SkeletonMeshComponent::Reflect(context);
        SkyBoxComponent::Reflect(context);

        static std::string GROUP = "Render";
        ComponentFactory::Get()->RegisterComponent<MainDirectLightComponent>(GROUP);
        ComponentFactory::Get()->RegisterComponent<StaticMeshComponent>(GROUP);
        ComponentFactory::Get()->RegisterComponent<SkeletonMeshComponent>(GROUP);
        ComponentFactory::Get()->RegisterComponent<CameraComponent>(GROUP);
        ComponentFactory::Get()->RegisterComponent<SkyBoxComponent>(GROUP);
    }

    void RenderModule::ProcessArgs(const StartArguments &args)
    {
        cxxopts::Options options("SkyEngine Render Module", "SkyEngine Render Module");
        options.allow_unrecognised_options();

        options.add_options()
            ("e,engine", "Engine Directory", cxxopts::value<std::string>())
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
        TextFeature::Get()->Init();
        LightFeature::Get()->Init();

        auto *am = AssetManager::Get();
        {
            auto guiAsset = am->LoadAssetFromPath<Technique>("techniques/gui.tech");
            guiAsset->BlockUntilLoaded();

            auto tech = CreateTechniqueFromAsset(guiAsset);
            ImGuiFeature::Get()->SetTechnique(tech);
        }
        {
            auto guiAsset = am->LoadAssetFromPath<Technique>("techniques/text.tech");
            guiAsset->BlockUntilLoaded();

            auto tech = CreateTechniqueFromAsset(guiAsset);
            TextFeature::Get()->SetTechnique(tech);
        }

    }

    void RenderModule::Shutdown()
    {
        Renderer::Get()->StopRender();

        MeshFeature::Destroy();
        ImGuiFeature::Destroy();
        TextFeature::Destroy();

        Renderer::Destroy();
        RHI::Destroy();
    }

    void RenderModule::Tick(float delta)
    {
        SKY_PROFILE_SCOPE;
        Renderer::Get()->Tick(delta);
    }
}
