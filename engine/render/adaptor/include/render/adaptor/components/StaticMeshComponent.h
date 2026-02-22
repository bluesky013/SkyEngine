//
// Created by Zach Lee on 2023/2/28.
//

#pragma once

#include <core/util/Uuid.h>
#include <core/math/Transform.h>
#include <framework/world/Component.h>
#include <framework/asset/AssetHolder.h>
#include <framework/interface/ITransformEvent.h>
#include <render/adaptor/assets/LodGroupAsset.h>
#include <render/mesh/MeshRenderer.h>

namespace sky {

    struct StaticMeshComponentData {
        bool castShadow = false;
        bool receiveShadow = false;

        Uuid resId;
    };

    class StaticMeshComponent
        : public ComponentAdaptor<StaticMeshComponentData>
        , public ITransformEvent
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

        void BuildLodGroupAsync();

        SingleAssetHolder<LodGroup> holder;
        EventBinder<ITransformEvent> transformEvent;
        MeshRenderer *renderer = nullptr;

        // transient status data
        Transform cachedTransform;

        // shared async
        RDLodGroupPtr cachedGroup;
        std::atomic_bool isMeshDirty = false;
    };

} // namespace receiveShadow
