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
        void OnTransformChanged(const Transform& global, const Transform& local) override;

        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

        void OnSerialized() override;
        void OnAssetLoaded(const Uuid& uuid) override;

        SingleAssetHolder<RenderPrefab> prefab;
        EventBinder<ITransformEvent> transformEvent;
    };

} // namespace receiveShadow
