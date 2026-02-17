//
// Created by blues on 2025/7/7.
//

#include <render/adaptor/animation/SkeletonDisplayComponent.h>
#include <render/adaptor/RenderSceneProxy.h>
#include <render/RenderTechniqueLibrary.h>
#include <framework/world/TransformComponent.h>

namespace sky {

    void SkeletonDisplayComponent::Reflect(SerializationContext *context)
    {
        context->Register<SkeletonDisplayComponent>("SkeletonDisplayComponent")
            .Member<&SkeletonDisplayComponent::SetSkeletonUuid, &SkeletonDisplayComponent::GetSkeletonUuid>("Skeleton")
                .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<Skeleton>::ASSET_TYPE));
    }

    void SkeletonDisplayComponent::SetSkeletonUuid(const Uuid &uuid)
    {
        const auto &current = skeletonAsset ? skeletonAsset->GetUuid() : Uuid::GetEmpty();
        if (current == uuid) {
            return;
        }

        if (uuid) {
            assetEvent.Bind(this, uuid);
        } else {
            assetEvent.Reset();
        }

        skeletonAsset = uuid ? AssetManager::Get()->LoadAsset<Skeleton>(uuid) : SkeletonAssetPtr {};
        if (skeletonAsset && skeletonAsset->IsLoaded()) {
            OnAssetLoaded();
        }
    }

    void SkeletonDisplayComponent::OnAssetLoaded()
    {
        const auto& data = skeletonAsset->Data();
        skeleton = Skeleton::BuildSkeleton(data);
    }

    void SkeletonDisplayComponent::Tick(float time)
    {
        if (dirty && skeleton != nullptr) {

            debugRender->DrawPose(*skeleton->GetRefPos(), cachedTransform);

            dirty = false;
        }
    }

    void SkeletonDisplayComponent::OnAttachToWorld()
    {
        if (!debugRender) {
            debugRender = std::make_unique<SkeletonDebugRender>();
            debugRender->SetTechnique(RenderTechniqueLibrary::Get()->FetchGfxTechnique(Name("techniques/debug.tech")));
        }

        auto *renderScene = static_cast<RenderSceneProxy*>(actor->GetWorld()->GetSubSystem(Name("RenderScene")))->GetRenderScene();
        renderScene->AddPrimitive(debugRender->GetPrimitive());

        dirty = true;
        transformEvent.Bind(this, actor);
        cachedTransform = actor->GetComponent<TransformComponent>()->GetWorldTransform();
    }

    void SkeletonDisplayComponent::OnDetachFromWorld()
    {
        auto *renderScene = static_cast<RenderSceneProxy*>(actor->GetWorld()->GetSubSystem(Name("RenderScene")))->GetRenderScene();
        renderScene->RemovePrimitive(debugRender->GetPrimitive());

        transformEvent.Reset();
    }

    void SkeletonDisplayComponent::OnTransformChanged(const Transform& global, const Transform& local)
    {
        dirty = true;
        cachedTransform = global;
    }

} // namespace sky