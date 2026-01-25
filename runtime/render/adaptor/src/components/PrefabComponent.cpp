//
// Created by Zach Lee on 2026/1/19.
//

#include <render/adaptor/components/PrefabComponent.h>

namespace sky {

    void PrefabComponent::Reflect(SerializationContext *context)
    {
        context->Register<PrefabComponentData>("PrefabComponentData")
            .Member<&PrefabComponentData::prefabId>("PrefabId");

        REGISTER_BEGIN(PrefabComponentData, context)
            REGISTER_MEMBER(Prefab, SetPrefabId, GetPrefabId)
                SET_ASSET_TYPE(AssetTraits<RenderPrefab>::ASSET_TYPE);
    }

    const Uuid& PrefabComponent::GetPrefabId() const
    {
        return data.prefabId;
    }

    void PrefabComponent::SetPrefabId(const Uuid& value)
    {
        data.prefabId = value;
    }

    void PrefabComponent::OnTransformChanged(const Transform& global, const Transform& local)
    {
    }

    void PrefabComponent::OnAttachToWorld()
    {
    }

    void PrefabComponent::OnDetachFromWorld()
    {
    }

    void PrefabComponent::OnSerialized()
    {
    }

    void PrefabComponent::OnAssetLoaded(const Uuid& uuid)
    {
    }

} // namespace sky
