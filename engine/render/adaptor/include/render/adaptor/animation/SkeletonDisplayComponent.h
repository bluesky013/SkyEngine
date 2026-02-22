//
// Created by blues on 2025/7/7.
//
#pragma once

#include <framework/world/Component.h>
#include <framework/asset/AssetEvent.h>
#include <framework/interface/ITransformEvent.h>
#include <render/adaptor/assets/SkeletonAsset.h>
#include <render/adaptor/animation/SkeletonDebugRender.h>

namespace sky {

    class SerializationContext;

    class SkeletonDisplayComponent
        : public ComponentBase
        , public ITransformEvent
        , public IAssetEvent {
    public:
        SkeletonDisplayComponent() = default;
        ~SkeletonDisplayComponent() override = default;

        COMPONENT_RUNTIME_INFO(SkeletonDisplayComponent)

        static void Reflect(SerializationContext *context);

        void SetSkeletonUuid(const Uuid &uuid);
        const Uuid& GetSkeletonUuid() const { return skeletonAsset ? skeletonAsset->GetUuid() : Uuid::GetEmpty(); }
    private:
        void OnAssetLoaded() override;

        void Tick(float time) override;

        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

        void OnTransformChanged(const Transform& global, const Transform& local) override;

        SkeletonAssetPtr skeletonAsset;
        SkeletonPtr skeleton;
        std::unique_ptr<SkeletonDebugRender> debugRender;

        EventBinder<IAssetEvent, Uuid> assetEvent;
        EventBinder<ITransformEvent> transformEvent;

        bool dirty = false;
        Transform cachedTransform;
    };

} // namespace sky
