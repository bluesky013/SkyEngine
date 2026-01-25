//
// Created by Zach Lee on 2026/1/19.
//

#include <render/adaptor/components/PrefabComponent.h>

namespace sky {

    void PrefabComponent::Reflect(SerializationContext *context)
    {
        context->Register<PrefabComponentData>("PrefabComponentData")
            .Member<&PrefabComponentData::prefabId>("PrefabId");

        REGISTER_BEGIN(PrefabComponent, context)
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
        prefab.SetAsset(value, this);

        if (isInWorld) {
            OnDetachFromWorld();
            OnAttachToWorld();
        }
    }

    void PrefabComponent::OnTransformChanged(const Transform& global, const Transform& local)
    {
    }

    void PrefabComponent::OnAttachToWorld()
    {
        isInWorld = true;
    }

    void PrefabComponent::OnDetachFromWorld()
    {
        isInWorld = false;
    }

    void PrefabComponent::OnSerialized()
    {
        prefab.SetAsset(data.prefabId, this);
    }

    void PrefabComponent::OnAssetLoaded(const Uuid& uuid, const std::string_view& type)
    {
        if (type == AssetTraits<RenderPrefab>::ASSET_TYPE) {
            auto data = prefab.Data();
            size_t nodeNum = data.nodes.size();
            std::unordered_set<Uuid> meshesToLoad;
            for (size_t i = 0; i < nodeNum; ++i) {
                const auto& node = data.nodes[i];
                meshesToLoad.emplace(node.mesh);
            }

            {
                std::lock_guard<std::mutex> lock(assetMutex);
                meshHolders.clear();
                for (auto& meshId : meshesToLoad) {
                    meshHolders[meshId].SetAsset(meshId, this);
                }
            }
        } else if (type == AssetTraits<Mesh>::ASSET_TYPE){


        }
    }

} // namespace sky
