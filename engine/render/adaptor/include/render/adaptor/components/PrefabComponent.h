//
// Created by Zach Lee on 2026/1/19.
//


#pragma once

#include <render/IStaticRenderObject.h>
#include <framework/asset/AssetHolder.h>
#include <framework/interface/ITransformEvent.h>
#include <framework/world/Component.h>
#include <render/adaptor/assets/RenderPrefab.h>
#include <render/lod/LodGroup.h>
#include <render/mesh/MeshRenderer.h>

namespace sky {

    struct PrefabComponentData {
        Uuid prefabId;
        std::vector<VisibleID> visibleIDs;
    };

    struct PrefabNodeProxy;

    class PrefabComponent
        : public ComponentAdaptor<PrefabComponentData>
        , public ITransformEvent
        , public IStaticRenderObjectGather
        , public IAssetReadyNotifier {
    public:
        PrefabComponent() = default;
        ~PrefabComponent() override = default;

        COMPONENT_RUNTIME_INFO(PrefabComponent)

        static void Reflect(SerializationContext *context);

        const Uuid& GetPrefabId() const;
        void SetPrefabId(const Uuid& value);

        void SetVisibleID(uint32_t nodeId, VisibleID visibleID);
        const Transform& GetCachedTransform() const { return cacheTransform; }

        const RenderPrefabAssetData& GetAssetData() const { return prefab.GetAsset()->Data(); }
    private:
        void Tick(float time) override;

        void OnTransformChanged(const Transform& global, const Transform& local) override;

        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

        void OnSerialized() override;
        void OnAssetLoaded(const Uuid& uuid, const std::string_view& type) override;

        void Gather(std::vector<IStaticRenderObject*> &outObjects) override;

        // logic threads
        std::vector<MeshRenderer*> renderers;
        EventBinder<ITransformEvent> transformEvent;
        EventBinder<IStaticRenderObjectGather> gatherEvent;
        SingleAssetHolder<RenderPrefab> prefab;
        Transform cacheTransform;

        // asset threads
        std::mutex assetMutex;
        std::unordered_map<Uuid, RDLodGroupPtr> lodGroups;
        std::unordered_map<Uuid, SingleAssetHolder<LodGroup>> lodGroupHolders;
        std::unordered_map<Uuid, std::vector<uint32_t>> linkList;

        struct MeshBuildTask {
            uint32_t nodeId;
            RDLodGroupPtr lodGroup;
            Transform localTransform;
        };

        // shared
        std::atomic_size_t capacity = 0;

        std::mutex meshMutex;
        std::vector<MeshBuildTask> buildTasks;

        std::vector<PrefabNodeProxy> proyNodes;

        bool isInWorld = false;
    };

    struct PrefabNodeProxy : IStaticRenderObject {
        uint32_t nodeId = 0;
        RDLodGroupPtr lodGroup;
        PrefabComponent* owner = nullptr;

        void SetObjectID(VisibleID id) override { owner->SetVisibleID(nodeId, id); }
        BoundingBoxSphere GetWorldBounds() const override
        {
            const auto& prefabData = owner->GetAssetData();
            const auto& trans = prefabData.nodes[nodeId].localTransform;
            Transform meshTrans = owner->GetCachedTransform() * trans;
            auto localBounds = lodGroup->GetBoundingBox();
            return BoundingBoxSphere::Transform(localBounds, meshTrans.ToMatrix());
        }

        Matrix4 GetWorldTransform() const override
        {
            const auto& prefabData = owner->GetAssetData();
            const auto& trans = prefabData.nodes[nodeId].localTransform;
            Transform meshTrans = owner->GetCachedTransform() * trans;
            return meshTrans.ToMatrix();
        }

        RDMeshPtr GetMesh() const override
        {
            return lodGroup->GetMesh();
        }
    };

} // namespace receiveShadow
