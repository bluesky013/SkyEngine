//
// Created by blues on 2024/12/8.
//

#include <render/adaptor/components/DecalComponent.h>

#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>

#include <render/adaptor/Util.h>
#include <render/decal/DecalFeatureProcessor.h>

namespace sky {

    void DecalComponent::Reflect(SerializationContext *context)
    {
        context->Register<DecalComponentData>("DecalComponentData")
            .Member<&DecalComponentData::color>("Color")
            .Member<&DecalComponentData::blendFactor>("BlendFactor");

        REGISTER_BEGIN(DecalComponent, context)
    }

    void DecalComponent::OnAttachToWorld()
    {
        auto *df = GetFeatureProcessor<DecalFeatureProcessor>(actor);
        if (df == nullptr) {
            return;
        }

        decal = df->CreateDecal();
        decal->SetColor(data.color);
        decal->SetBlendFactor(data.blendFactor);

        transformEvent.Bind(this, actor);

        auto *transform = actor->GetComponent<TransformComponent>();
        if (transform != nullptr) {
            UpdateDecal(transform->GetWorldTransform());
        }
    }

    void DecalComponent::OnDetachFromWorld()
    {
        auto *df = GetFeatureProcessor<DecalFeatureProcessor>(actor);
        if (df != nullptr && decal != nullptr) {
            df->RemoveDecal(decal);
        }
        decal = nullptr;
        transformEvent.Reset();
    }

    void DecalComponent::OnTransformChanged(const Transform &global, const Transform &/*local*/)
    {
        UpdateDecal(global);
    }

    void DecalComponent::UpdateDecal(const Transform &transform)
    {
        if (decal == nullptr) {
            return;
        }
        decal->SetWorldMatrix(transform.ToMatrix());
        decal->SetColor(data.color);
        decal->SetBlendFactor(data.blendFactor);
    }

} // namespace sky
