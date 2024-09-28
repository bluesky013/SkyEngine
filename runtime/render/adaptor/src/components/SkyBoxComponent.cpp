//
// Created by blues on 2024/9/28.
//

#include <render/adaptor/components/SkyBoxComponent.h>
#include <render/adaptor/Util.h>
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
            .Member<&SkyBoxData::skybox>("skybox");

        REGISTER_BEGIN(SkyBoxComponent, context)
            REGISTER_MEMBER(texture, SetImage, GetImage)
                SET_ASSET_TYPE(AssetTraits<Texture>::ASSET_TYPE);
    }

    void SkyBoxComponent::Tick(float time)
    {
        if (texture && texture->IsReady() && renderer) {
            renderer->SetReady();
        }
    }

    void SkyBoxComponent::OnAttachToWorld()
    {
        if (data.skybox && !texture) {
            SetImage(data.skybox);
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

        texture = CreateTextureFromAsset(texAsset);
        Renderer::Get()->GetStreamingManager()->UploadTexture(texture);

        renderer->GetResGroup()->BindTexture("SkyBox", texture->GetImageView(), 0);
        renderer->GetResGroup()->Update();
    }

    const Uuid &SkyBoxComponent::GetImage() const
    {
        return data.skybox;
    }

} // namespace sky