//
// Created by Zach Lee on 2026/2/24.
//

#include <pvs/editor/PVSVolume.h>

#include <framework/asset/AssetManager.h>
#include <framework/world/TransformComponent.h>

#include <render/adaptor/assets/TechniqueAsset.h>
#include <render/adaptor/Util.h>

namespace sky::editor {

    void PVSVolume::Reflect(SerializationContext *context)
    {
        REGISTER_BEGIN(PVSVolume, context);
    }

    void PVSVolume::OnAttachToWorld()
    {
        const auto &global = actor->GetComponent<TransformComponent>()->GetWorldTransform();
        boundingBox.center = global.translation;
        boundingBox.extent = global.scale * 0.5f;;

        if (volumeRender == nullptr) {
            auto boxTech = AssetManager::Get()->LoadAssetFromPath<Technique>("techniques/box.tech");
            boxTech->BlockUntilLoaded();

            auto *renderScene = GetRenderSceneFromActor(actor);
            volumeRender = std::make_unique<VolumeRenderer>(renderScene, CreateGfxTechFromAsset(boxTech));
        }
        DrwBox();

        transformEvent.Bind(this, actor);
    }

    void PVSVolume::OnDetachFromWorld()
    {
        volumeRender = nullptr;
        transformEvent.Reset();
    }

    void PVSVolume::OnTransformChanged(const Transform& global, const Transform& local)
    {
        boundingBox.center = global.translation;
        boundingBox.extent = global.scale * 0.5f;
        DrwBox();
    }

    void PVSVolume::DrwBox() noexcept
    {
        if (volumeRender != nullptr) {
            volumeRender->Draw(boundingBox);
            volumeRender->Update();
        }
    }

    bool PVSVolumeCreator::OnCreateActor(Actor* actor)
    {
        return true;
    }
} // namespace sky::editor