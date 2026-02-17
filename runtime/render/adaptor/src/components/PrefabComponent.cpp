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
            if (renderer != nullptr) {
                meshFeature->RemoveStaticMesh(renderer);
            }

            Transform meshTrans = cacheTransform * task.localTransform;

            renderer = meshFeature->CreateStaticMesh();
            renderer->SetMesh(task.mesh, false);
            renderer->UpdateTransform(meshTrans.ToMatrix());

            renderers[task.nodeId] = renderer;
        }
    }


    void PrefabComponent::OnAttachToWorld()
    {
        isInWorld = true;
        transformEvent.Bind(this, actor);
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
    }

    void PrefabComponent::OnSerialized()
    {
        prefab.SetAsset(data.prefabId, this);
    }

    void PrefabComponent::OnAssetLoaded(const Uuid& uuid, const std::string_view& type)
    {
        if (type == AssetTraits<RenderPrefab>::ASSET_TYPE) {
            const auto &data = prefab.Data();
            size_t nodeNum = data.nodes.size();
            capacity.store(nodeNum);
            linkList.clear();
            std::unordered_set<Uuid> meshesToLoad;
            for (uint32_t i = 0; i < nodeNum; ++i) {
                const auto& node = data.nodes[i];
                meshesToLoad.emplace(node.mesh);
                linkList[node.mesh].emplace_back(i);
            }

            meshHolders.clear();
            for (const auto& meshId : meshesToLoad) {
                meshHolders.emplace(meshId, SingleAssetHolder<Mesh>{});
            }

            for (auto& [id, holder] : meshHolders) {
                holder.SetAsset(id, this);
            }

        } else if (type == AssetTraits<Mesh>::ASSET_TYPE){
            auto iter = meshHolders.find(uuid);
            if (iter != meshHolders.end()) {
                auto mesh = CreateMeshFromAsset(meshHolders[uuid].GetAsset());
                meshes[uuid] = mesh;

                for (auto& nodeId : linkList[uuid]) {
                    std::lock_guard<std::mutex> lock(meshMutex);
                    buildTasks.emplace_back();
                    auto& buildTask = buildTasks.back();
                    buildTask.mesh = mesh;
                    buildTask.nodeId = nodeId;
                    buildTask.localTransform = prefab.Data().nodes[nodeId].localTransform;
                }
            }
        }
    }

} // namespace sky
