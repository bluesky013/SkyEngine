//
// Created by Zach Lee on 2023/2/28.
//

#pragma once

#include <render/IStaticRenderObject.h>
#include <core/math/Transform.h>
#include <core/util/Uuid.h>
#include <framework/asset/AssetHolder.h>
#include <framework/interface/ITransformEvent.h>
#include <framework/world/Component.h>
#include <render/adaptor/assets/LodGroupAsset.h>
#include <render/mesh/MeshRenderer.h>

namespace sky {

    struct StaticMeshComponentData {
        bool castShadow = false;
        bool receiveShadow = false;

        Uuid resId;
        VisibleID visibleID = ~(0U);
    };

    class StaticMeshComponent
        : public ComponentAdaptor<StaticMeshComponentData>
        , public ITransformEvent
        , public IStaticRenderObject
        , public IStaticRenderObjectGather
        , public IAssetReadyNotifier {
    public:
        StaticMeshComponent() = default;
        ~StaticMeshComponent() override = default;

        COMPONENT_RUNTIME_INFO(StaticMeshComponent)

        static void Reflect(SerializationContext *context);

        void Tick(float time) override;
        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;
    private:
        void SetMeshUuid(const Uuid &uuid);
        const Uuid& GetMeshUuid() const { return data.resId; }

        void OnSerialized() override;

        void OnAssetLoaded(const Uuid& uuid, const std::string_view& type) override;

        void OnTransformChanged(const Transform& global, const Transform& local) override;

        VisibleID GetVisibleID() const { return data.visibleID; }
        void SetObjectID(VisibleID id) override;

        BoundingBoxSphere GetWorldBounds() const override;
        Matrix4 GetWorldTransform() const override;
        RDMeshPtr GetMesh() const override;

        void Gather(std::vector<IStaticRenderObject*> &outObjects) override;

        void BuildLodGroupAsync();

        SingleAssetHolder<LodGroup> holder;
        EventBinder<ITransformEvent> transformEvent;
        EventBinder<IStaticRenderObjectGather> staticObjectEvent;

        MeshRenderer *renderer = nullptr;

        // transient status data
        Transform cachedTransform;

        // shared async
        RDLodGroupPtr cachedGroup;
        std::atomic_bool isMeshDirty = false;
    };

} // namespace receiveShadow
