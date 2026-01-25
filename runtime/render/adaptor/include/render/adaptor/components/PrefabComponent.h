//
// Created by Zach Lee on 2026/1/19.
//


#pragma once

#include <framework/world/Component.h>
#include <framework/asset/AssetEvent.h>
#include <framework/asset/AssetHolder.h>
#include <framework/interface/ITransformEvent.h>
#include <render/adaptor/assets/RenderPrefab.h>
#include <render/resource/Mesh.h>
#include <render/mesh/MeshRenderer.h>

namespace sky {

    struct PrefabComponentData {
        Uuid prefabId;
    };

    class PrefabComponent
        : public ComponentAdaptor<PrefabComponentData>
        , public ITransformEvent
        , public IAssetReadyNotifier {
    public:
        PrefabComponent() = default;
        ~PrefabComponent() override = default;

        COMPONENT_RUNTIME_INFO(PrefabComponent)

        static void Reflect(SerializationContext *context);

        const Uuid& GetPrefabId() const;
        void SetPrefabId(const Uuid& value);

    private:
        void Tick(float time) override;

        void OnTransformChanged(const Transform& global, const Transform& local) override;

        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

        void OnSerialized() override;
        void OnAssetLoaded(const Uuid& uuid, const std::string_view& type) override;

        // logic threads
        std::vector<MeshRenderer*> renderers;
        EventBinder<ITransformEvent> transformEvent;
        SingleAssetHolder<RenderPrefab> prefab;
        Transform cacheTransform;

        // asset threads
        std::unordered_map<Uuid, RDMeshPtr> meshes;
        std::unordered_map<Uuid, SingleAssetHolder<Mesh>> meshHolders;
        std::unordered_map<Uuid, std::vector<uint32_t>> linkList;

        struct MeshBuildTask {
            uint32_t nodeId;
            RDMeshPtr mesh;
            Transform localTransform;
        };

        // shared
        std::atomic_size_t capacity = 0;

        std::mutex meshMutex;
        std::vector<MeshBuildTask> buildTasks;

        bool isInWorld = false;
    };

} // namespace receiveShadow
