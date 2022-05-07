//
// Created by Zach Lee on 2021/12/15.
//

#include <engine/render/Render.h>
#include <engine/render/camera/CameraComponent.h>
#include <engine/render/light/LightComponent.h>
#include <engine/render/model/MeshComponent.h>
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