//
// Created by Zach Lee on 2023/2/20.
//

#include <framework/interface/IModule.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>

#include <render/assets/Material.h>
#include <render/assets/Shader.h>
#include <render/assets/Technique.h>

namespace sky {

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
        serializationContext->Register<ShaderAssetData>("ShaderAssetData")
            .BinLoad<&ShaderAssetData::Load>()
            .BinSave<&ShaderAssetData::Save>();

        serializationContext->Register<MaterialAssetData>("MaterialAssetData")
            .BinLoad<&MaterialAssetData::Load>()
            .BinSave<&MaterialAssetData::Save>();

        serializationContext->Register<MaterialInstanceAssetData>("MaterialInstanceAssetData")
            .BinLoad<&MaterialInstanceAssetData::Load>()
            .BinSave<&MaterialInstanceAssetData::Save>();

        serializationContext->Register<TechniqueAssetData>("TechniqueAssetData")
            .BinLoad<&TechniqueAssetData::Load>()
            .BinSave<&TechniqueAssetData::Save>();

        auto *am = AssetManager::Get();
        am->RegisterAssetHandler<Shader>();
        am->RegisterAssetHandler<Material>();
        am->RegisterAssetHandler<MaterialInstance>();
        am->RegisterAssetHandler<Technique>();
    }
}
REGISTER_MODULE(sky::RenderModule)