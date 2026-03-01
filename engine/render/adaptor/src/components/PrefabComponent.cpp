//
// Created by Zach Lee on 2026/1/19.
//

#include <render/adaptor/components/PrefabComponent.h>
#include <render/adaptor/Util.h>
#include <render/mesh/MeshFeatureProcessor.h>

namespace sky {

    void PrefabComponent::Reflect(SerializationContext *context)
    {
        context->Register<PrefabComponentData>("PrefabComponentData")
            .Member<&PrefabComponentData::prefabId>("PrefabId")
            .Member<&PrefabComponentData::visibleIDs>("VisibleIds");


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

    void PrefabComponent::SetVisibleID(uint32_t nodeId, VisibleID visibleID)
    {
        data.visibleIDs[nodeId] = visibleID;
    }

    void PrefabComponent::OnTransformChanged(const Transform& global, const Transform& local)
    {
        cacheTransform = global;

        auto size = renderers.size();
        for (int i = 0; i < size; i++) {
            auto* renderer = renderers[i];
            const auto& trans = prefab.Data().nodes[i].localTransform;

            if (renderer != nullptr) {
                Transform meshTrans = cacheTransform * trans;
                renderer->UpdateTransform(meshTrans.ToMatrix());
            }
        }
    }

    void PrefabComponent::Tick(float time)
    {
        // adjust capacity
        auto *meshFeature = GetFeatureProcessor<MeshFeatureProcessor>(actor);
        {
            size_t currentCapacity = capacity.load();
            renderers.resize(currentCapacity);
        }

        std::vector<MeshBuildTask> tasks;
        {
            std::lock_guard<std::mutex> lock(meshMutex);
            tasks.swap(buildTasks);
        }

        for (auto& task : tasks) {
            auto *renderer = renderers[task.nodeId];
            if (renderer == nullptr) {
                renderer = meshFeature->CreateStaticMesh();
                Transform meshTrans = cacheTransform * task.localTransform;
                renderer->SetMeshLodGroup(task.lodGroup);
                renderer->SetUniqueID(data.visibleIDs[task.nodeId]);
                renderer->UpdateTransform(meshTrans.ToMatrix());
            }

            renderers[task.nodeId] = renderer;
        }
    }


    void PrefabComponent::OnAttachToWorld()
    {
        isInWorld = true;
        transformEvent.Bind(this, actor);
        gatherEvent.Bind(this, actor);
    }

    void PrefabComponent::OnDetachFromWorld()
    {
        auto *meshFeature = GetFeatureProcessor<MeshFeatureProcessor>(actor);
        for (auto* renderer : renderers) {
            if (renderer != nullptr) {
                meshFeature->RemoveStaticMesh(renderer);
            }
        }
        renderers.clear();
        isInWorld = false;
        transformEvent.Reset();
        gatherEvent.Reset();
    }

    void PrefabComponent::OnSerialized()
    {
        prefab.SetAsset(data.prefabId, this);
    }

    void PrefabComponent::OnAssetLoaded(const Uuid& uuid, const std::string_view& type)
    {
        if (type == AssetTraits<RenderPrefab>::ASSET_TYPE) {
            const auto &prefabData = prefab.Data();
            size_t nodeNum = prefabData.nodes.size();
            capacity.store(nodeNum);
            proyNodes.resize(nodeNum);
            data.visibleIDs.resize(nodeNum, ~(0U));
            linkList.clear();
            std::unordered_set<Uuid> meshesToLoad;
            for (uint32_t i = 0; i < nodeNum; ++i) {
                const auto& node = prefabData.nodes[i];
                meshesToLoad.emplace(node.mesh);
                linkList[node.mesh].emplace_back(i);

                proyNodes[i].nodeId = i;
                proyNodes[i].owner = this;
            }

            lodGroupHolders.clear();
            for (const auto& meshId : meshesToLoad) {
                lodGroupHolders.emplace(meshId, SingleAssetHolder<LodGroup>{});
            }

            std::lock_guard<std::mutex> lock(assetMutex);
            for (auto& [id, holder] : lodGroupHolders) {
                holder.SetAsset(id, this);
            }

        } else if (type == AssetTraits<LodGroup>::ASSET_TYPE){
            std::lock_guard<std::mutex> lockHolder(assetMutex);
            auto iter = lodGroupHolders.find(uuid);
            if (iter != lodGroupHolders.end()) {
                auto lodGroup = CreateLodGroupFromAsset(lodGroupHolders[uuid].GetAsset());
                lodGroups[uuid] = lodGroup.group;

                for (auto& nodeId : linkList[uuid]) {
                    std::lock_guard<std::mutex> lock(meshMutex);
                    buildTasks.emplace_back();
                    auto& buildTask = buildTasks.back();
                    buildTask.lodGroup = lodGroup.group;
                    buildTask.nodeId = nodeId;
                    buildTask.localTransform = prefab.Data().nodes[nodeId].localTransform;

                    proyNodes[nodeId].lodGroup = lodGroup.group;
                }
            }
        }
    }

    void PrefabComponent::Gather(std::vector<IStaticRenderObject*> &outObjects)
    {
        for (auto& node : proyNodes) {
            outObjects.emplace_back(&node);
        }
    }

} // namespace sky
