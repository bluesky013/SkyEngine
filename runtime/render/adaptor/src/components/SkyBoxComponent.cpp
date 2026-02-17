//
// Created by blues on 2024/9/28.
//

#include <render/adaptor/components/SkyBoxComponent.h>
#include <render/adaptor/Util.h>
#include <render/env/EnvFeature.h>
#include <render/Renderer.h>
#include <framework/world/ComponentFactory.h>
#include <framework/serialization/PropertyCommon.h>

namespace sky {

    SkyBoxComponent::SkyBoxComponent()
    {
        techAsset = AssetManager::Get()->LoadAssetFromPath<Technique>("techniques/skybox.tech");
        techAsset->BlockUntilLoaded();
        technique = CreateTechniqueFromAsset(techAsset);

        renderer = std::make_unique<SkySphereRenderer>();
        renderer->SetTechnique(technique);
    }

    void SkyBoxComponent::Reflect(SerializationContext *context)
    {
        context->Register<SkyBoxData>("SkyBoxData")
            .Member<&SkyBoxData::skybox>("Skybox")
            .Member<&SkyBoxData::radiance>("Radiance")
            .Member<&SkyBoxData::irradiance>("Irradiance");

        REGISTER_BEGIN(SkyBoxComponent, context)
            REGISTER_MEMBER(Texture, SetImage, GetImage)
                SET_ASSET_TYPE(AssetTraits<Texture>::ASSET_TYPE)
            REGISTER_MEMBER(Radiance, SetRadiance, GetRadiance)
                SET_ASSET_TYPE(AssetTraits<Texture>::ASSET_TYPE)
            REGISTER_MEMBER(Irradiance, SetIrradiance, GetIrradiance)
                SET_ASSET_TYPE(AssetTraits<Texture>::ASSET_TYPE);
    }

    void SkyBoxComponent::Tick(float time)
    {
    }

    void SkyBoxComponent::OnAttachToWorld()
    {
        if (data.skybox && !texture) {
            SetImage(data.skybox);
        }

        if (data.radiance && !radiance) {
            SetRadiance(data.radiance);
        }

        if (data.irradiance && !irradiance) {
            SetIrradiance(data.irradiance);
        }

        auto *scene = GetRenderSceneFromActor(actor);
        scene->AddPrimitive(renderer->GetPrimitive());
    }

    void SkyBoxComponent::OnDetachFromWorld()
    {
        auto *scene = GetRenderSceneFromActor(actor);
        scene->RemovePrimitive(renderer->GetPrimitive());
        renderer = nullptr;
    }

    void SkyBoxComponent::SetImage(const Uuid &image)
    {
        data.skybox = image;
        auto texAsset = AssetManager::Get()->LoadAsset<Texture>(image);
        texAsset->BlockUntilLoaded();

        texture = CastPtr<Texture2D>(CreateTextureFromAsset(texAsset));
        Renderer::Get()->GetStreamingManager()->UploadTexture(texture);
        renderer->GetPrimitive()->texture = texture;
    }

    void SkyBoxComponent::SetRadiance(const Uuid &image)
    {
        data.radiance = image;

        auto texAsset = AssetManager::Get()->LoadAsset<Texture>(image);
        texAsset->BlockUntilLoaded();

        radiance = CastPtr<TextureCube>(CreateTextureFromAsset(texAsset));
        Renderer::Get()->GetStreamingManager()->UploadTexture(radiance);

        auto *ef = GetFeatureProcessor<EnvFeatureProcessor>(actor);
        if (radiance) {
            ef->SetRadiance(radiance);
        }
    }

    void SkyBoxComponent::SetIrradiance(const Uuid &image)
    {
        data.irradiance = image;

        auto texAsset = AssetManager::Get()->LoadAsset<Texture>(image);
        texAsset->BlockUntilLoaded();

        irradiance = CastPtr<TextureCube>(CreateTextureFromAsset(texAsset));
        Renderer::Get()->GetStreamingManager()->UploadTexture(irradiance);

        auto *ef = GetFeatureProcessor<EnvFeatureProcessor>(actor);
        if (irradiance) {
            ef->SetIrradiance(irradiance);
        }
    }

    const Uuid &SkyBoxComponent::GetImage() const
    {
        return data.skybox;
    }

    const Uuid &SkyBoxComponent::GetRadiance() const
    {
        return data.radiance;
    }

    const Uuid &SkyBoxComponent::GetIrradiance() const
    {
        return data.irradiance;
    }

} // namespace sky