//
// Created by blues on 2024/8/11.
//

#pragma once

#include <framework/asset/AssetEvent.h>
#include <framework/asset/AssetHolder.h>
#include <framework/interface/ITransformEvent.h>
#include <framework/world/Component.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/adaptor/event/PoseUpdateEvent.h>
#include <render/resource/Mesh.h>
#include <render/skeleton/SkeletalMeshRenderer.h>
#include <animation/graph/AnimationNode.h>

namespace sky {

    struct SkeletonMeshComponentData {
        Uuid mesh;
    };

    class SkeletalMeshComponent
        : public ComponentAdaptor<SkeletonMeshComponentData>
        , public ITransformEvent
        , public IPoseUpdateEvent
        , public IAssetReadyNotifier {
    public:
        SkeletalMeshComponent() = default;
        ~SkeletalMeshComponent() override;

        COMPONENT_RUNTIME_INFO(SkeletalMeshComponent)

        static void Reflect(SerializationContext *context);

        void Tick(float time) override;
        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

        void SetMeshUuid(const Uuid &uuid);
        const Uuid& GetMeshUuid() const { return data.mesh; }
    private:
        void OnSerialized() override;

        void OnAssetLoaded(const Uuid& uuid, const std::string_view& type) override;

        void OnTransformChanged(const Transform& global, const Transform& local) override;

        void OnPoseUpdated(const AnimFinalPose& pose) override;

        void BuildSkeletonMeshAsync();

        SingleAssetHolder<Mesh> holder;
        EventBinder<ITransformEvent> transformEvent;
        EventBinder<IPoseUpdateEvent> poseEvent;
        SkeletalMeshRenderer *renderer = nullptr;

        // transient status data
        Transform cachedTransform;

        // shared async
        CounterPtr<Mesh> cachedMesh;
        std::atomic_uint32_t isMeshDirty = false;
    };

} // namespace receiveShadow
