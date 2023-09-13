//
// Created by Zach Lee on 2023/2/28.
//

#include <render/adaptor/components/MeshRenderer.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/asset/AssetManager.h>
#include <render/adaptor/Util.h>
#include <render/mesh/MeshFeatureProcessor.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/GameObject.h>

namespace sky {

    void MeshRenderer::Reflect()
    {
        SerializationContext::Get()
            ->Register<MeshRenderer>(NAME)
            .Member<&MeshRenderer::isStatic>("static")
            .Member<&MeshRenderer::castShadow>("castShadow")
            .Member<&MeshRenderer::receiveShadow>("receiveShadow");

        ComponentFactory::Get()->RegisterComponent<MeshRenderer>();
    }

    void MeshRenderer::Save(JsonOutputArchive &ar) const
    {
        ar.StartObject();
        ar.SaveValueObject(std::string("static"), isStatic);
        ar.SaveValueObject(std::string("castShadow"), castShadow);
        ar.SaveValueObject(std::string("receiveShadow"), receiveShadow);
        ar.SaveValueObject(std::string("mesh"), meshAsset ? meshAsset->GetUuid() : Uuid());
        ar.EndObject();
    }

    void MeshRenderer::Load(JsonInputArchive &ar)
    {
        ar.LoadKeyValue("static", isStatic);
        ar.LoadKeyValue("castShadow", castShadow);
        ar.LoadKeyValue("receiveShadow", receiveShadow);
        Uuid uuid;
        ar.LoadKeyValue("mesh", uuid);
        meshAsset = AssetManager::Get()->LoadAsset<Mesh>(uuid);
        ResetMesh();
    }

    void MeshRenderer::SetMesh(const MeshAssetPtr &mesh_)
    {
        meshAsset = mesh_;
        ResetMesh();
    }

    void MeshRenderer::ResetMesh()
    {
        if (renderer == nullptr) {
            auto *mf = GetFeatureProcessor<MeshFeatureProcessor>(GetRenderSceneFromGameObject(object));
            renderer = mf->CreateStaticMesh();
        }
        renderer->SetMesh(meshAsset->CreateInstance());
    }

    void MeshRenderer::OnActive()
    {
    }

    void MeshRenderer::OnDestroy()
    {
        GetFeatureProcessor<MeshFeatureProcessor>(GetRenderSceneFromGameObject(object))->RemoveStaticMesh(renderer);
    }

    void MeshRenderer::OnTick(float time)
    {
        if (renderer != nullptr) {
            auto *ts = object->GetComponent<TransformComponent>();
            renderer->UpdateTransform(ts->GetWorld().ToMatrix());
        }
    }
} // namespace sky