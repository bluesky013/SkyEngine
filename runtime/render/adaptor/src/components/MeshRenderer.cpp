//
// Created by Zach Lee on 2023/2/28.
//

#include <framework/asset/AssetManager.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>
#include <render/adaptor/Util.h>
#include <render/adaptor/components/MeshRenderer.h>
#include <render/mesh/MeshFeatureProcessor.h>

namespace sky {

    MeshRenderer::~MeshRenderer()
    {
        ShutDown();
    }

    void MeshRenderer::Reflect(SerializationContext *context)
    {
        context->Register<MeshRenderer>("MeshRenderer")
            .Member<&MeshRenderer::isStatic>("static")
            .Member<&MeshRenderer::SetMeshUuid, &MeshRenderer::GetMeshUuid>("mesh");
    }

    void MeshRenderer::SaveJson(JsonOutputArchive &ar) const
    {
        ar.StartObject();
        ar.SaveValueObject(std::string("static"), isStatic);
        ar.SaveValueObject(std::string("castShadow"), castShadow);
        ar.SaveValueObject(std::string("receiveShadow"), receiveShadow);
        ar.SaveValueObject(std::string("mesh"), meshAsset ? meshAsset->GetUuid() : Uuid());
        ar.EndObject();
    }

    void MeshRenderer::LoadJson(JsonInputArchive &ar)
    {
        ar.LoadKeyValue("static", isStatic);
        ar.LoadKeyValue("castShadow", castShadow);
        ar.LoadKeyValue("receiveShadow", receiveShadow);
        Uuid uuid;
        ar.LoadKeyValue("mesh", uuid);
        meshAsset = AssetManager::Get()->LoadAsset<Mesh>(uuid);
//        ResetMesh();
    }

    void MeshRenderer::SetMeshUuid(const Uuid &uuid)
    {
        const auto &current = meshAsset ? meshAsset->GetUuid() : Uuid::GetEmpty();
        if (current == uuid) {
            return;
        }
        if (uuid) {
            SetMesh(AssetManager::Get()->LoadAsset<Mesh>(uuid));
        } else {
            SetMesh(MeshAssetPtr {});
        }
    }

    void MeshRenderer::SetMesh(const MeshAssetPtr &mesh_)
    {
        meshAsset = mesh_;
        if (meshAsset) {
            meshInstance = meshAsset->CreateInstance();
        } else {
            meshInstance = nullptr;
        }

        ResetMesh();
    }

    void MeshRenderer::SetMesh(const RDMeshPtr &mesh)
    {
        meshInstance = mesh;
        ResetMesh();
    }

    void MeshRenderer::ResetMesh()
    {
        auto *mf = GetFeatureProcessor<MeshFeatureProcessor>(actor);

        if (!meshInstance) {
            if (renderer != nullptr) {
                mf->RemoveStaticMesh(renderer);
            }
            renderer = nullptr;
            return;
        }

        if (renderer == nullptr) {
            renderer = mf->CreateStaticMesh();
        }
        renderer->SetMesh(meshInstance);
    }

    void MeshRenderer::ShutDown()
    {
        if (renderer != nullptr) {
            GetFeatureProcessor<MeshFeatureProcessor>(actor)->RemoveStaticMesh(renderer);
            renderer = nullptr;
        }
    }

    void MeshRenderer::OnActive()
    {
    }

    void MeshRenderer::OnDeActive()
    {
        ShutDown();
    }

    void MeshRenderer::Tick(float time)
    {
        if (renderer != nullptr) {
            auto *ts = actor->GetComponent<TransformComponent>();
            renderer->UpdateTransform(ts->GetWorldMatrix());
        }
    }
} // namespace sky