//
// Created by Zach Lee on 2022/8/25.
//

#include <cereal/archives/json.hpp>
#include <framework/asset/AssetManager.h>
#include <render/resources/Prefab.h>
#include <render/RenderScene.h>
#include <render/features/StaticMeshFeature.h>

namespace sky {

    template <typename T>
    std::shared_ptr<Asset<T>> LoadAsset(const Uuid &id, const PrefabData &data)
    {
        auto iter = data.assetPathMap.find(id);
        if (iter != data.assetPathMap.end()) {
            return AssetManager::Get()->LoadAsset<T>(iter->second);
        }
        return nullptr;
    }

    std::shared_ptr<Prefab> Prefab::CreateFromData(const PrefabData &data)
    {
        auto prefab = std::make_shared<Prefab>();
        prefab->nodes = data.nodes;

        prefab->buffers.reserve(data.buffers.size());
        for (auto& buffer : data.buffers) {
            prefab->buffers.emplace_back(LoadAsset<Buffer>(buffer, data));
        }

        for (auto& mesh : data.meshes) {
            prefab->meshes.emplace_back(LoadAsset<Mesh>(mesh, data));
        }

        for (auto& image : data.images) {
            prefab->images.emplace_back(LoadAsset<Image>(image, data));
        }

        return prefab;
    }

    void Prefab::LoadToScene(RenderScene &scene)
    {
        auto        pass        = std::make_shared<Pass>();
        SubPassInfo subPassInfo = {};
        subPassInfo.colors.emplace_back(AttachmentInfo{VK_FORMAT_B8G8R8A8_UNORM, VK_SAMPLE_COUNT_4_BIT});
        subPassInfo.depthStencil = AttachmentInfo{VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_4_BIT};
        pass->AddSubPass(subPassInfo);
        pass->InitRHI();

        auto colorTechAsset = AssetManager::Get()->LoadAsset<GraphicsTechnique>("data\\techniques\\base_color_forward.tech");
        auto colorTech = colorTechAsset->CreateInstance();
        colorTech->SetRenderPass(pass);
        colorTech->SetViewTag(MAIN_CAMERA_TAG);
        colorTech->SetDrawTag(FORWARD_TAG);

        auto material = std::make_shared<Material>();
        material->AddGfxTechnique(colorTech);
        material->InitRHI();

        material->UpdateValue("material.baseColor", Vector4{1.f, 1.f, 1.f, 1.f});
        material->Update();


        auto *smFeature  = scene.GetFeature<StaticMeshFeature>();
        for (auto& node : nodes) {
            if (node.meshIndex != ~(0u)) {
                StaticMesh *staticMesh = smFeature->Create();
                RDMeshPtr mesh = meshes[node.meshIndex]->CreateInstance();
                mesh->SetMaterial(material, 0);
                staticMesh->SetMesh(mesh);
                staticMesh->SetWorldMatrix(node.transform);
            }
        }
    }
}
