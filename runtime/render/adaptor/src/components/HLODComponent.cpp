//
// Created by Copilot on 2026/2/16.
//

#include <render/adaptor/components/HLODComponent.h>

#include <framework/serialization/JsonArchive.h>
#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>

#include <render/adaptor/Util.h>
#include <render/hlod/HLODFeatureProcessor.h>

namespace sky {

    HLODComponent::~HLODComponent()
    {
        ShutDown();
    }

    void HLODComponent::Reflect(SerializationContext *context)
    {
        context->Register<HLODComponent>("HLODComponent");
    }

    void HLODComponent::SaveJson(JsonOutputArchive &ar) const
    {
        ar.StartObject();
        ar.EndObject();
    }

    void HLODComponent::LoadJson(JsonInputArchive &ar)
    {
    }

    void HLODComponent::SetHLODTree(const HLODTreePtr &tree)
    {
        hlodTree = tree;
        if (renderer != nullptr) {
            renderer->SetHLODTree(hlodTree);
        }
    }

    void HLODComponent::BuildRenderer()
    {
        auto *fp = GetFeatureProcessor<HLODFeatureProcessor>(actor);

        if (renderer != nullptr) {
            fp->RemoveHLODRenderer(renderer);
        }

        renderer = fp->CreateHLODRenderer();
        if (hlodTree) {
            renderer->SetHLODTree(hlodTree);
        }
    }

    void HLODComponent::ShutDown()
    {
        if (renderer != nullptr) {
            GetFeatureProcessor<HLODFeatureProcessor>(actor)->RemoveHLODRenderer(renderer);
            renderer = nullptr;
        }
    }

    void HLODComponent::Tick(float time)
    {
        if (renderer == nullptr) {
            BuildRenderer();
        }

        if (renderer != nullptr) {
            auto *ts = actor->GetComponent<TransformComponent>();
            renderer->UpdateTransform(ts->GetWorldMatrix());

            auto *renderScene = GetRenderSceneFromActor(actor);
            auto *sceneView = renderScene->GetSceneView(Name("MainCamera"));
            if (sceneView != nullptr) {
                const auto &col = sceneView->GetWorld()[3];
                renderer->UpdateLODSelection(Vector3{col.x, col.y, col.z});
            }
        }
    }

    void HLODComponent::OnAttachToWorld()
    {
    }

    void HLODComponent::OnDetachFromWorld()
    {
        ShutDown();
    }

} // namespace sky
