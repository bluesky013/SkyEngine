//
// Created by Zach Lee on 2021/12/15.
//

#include <engine/render/Render.h>
#include <engine/feature/camera/CameraComponent.h>
#include <engine/feature/light/LightComponent.h>
#include <engine/feature/model/MeshComponent.h>
#include <framework/serialization/SerializationContext.h>

namespace sky {

    void CameraComponent::Reflect()
    {
        SerializationContext::Get()->Register<CameraComponent>(TypeName())
            .Member<&CameraComponent::near>("near")
            .Member<&CameraComponent::far>("far");
    }

    void LightComponent::Reflect()
    {
        SerializationContext::Get()->Register<LightComponent>(TypeName());
    }

    void MeshComponent::Reflect()
    {
        SerializationContext::Get()->Register<MeshComponent>(TypeName());
    }

    void Render::Reflect()
    {
        CameraComponent::Reflect();
        LightComponent::Reflect();
        MeshComponent::Reflect();
    }

}