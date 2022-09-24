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

        for (auto& mat : data.materials) {
            prefab->materials.emplace_back(LoadAsset<Material>(mat, data));
        }

        for (auto& image : data.images) {
            prefab->images.emplace_back(LoadAsset<Image>(image, data));
        }

        for (auto& mesh : data.meshes) {
            prefab->meshes.emplace_back(LoadAsset<Mesh>(mesh, data));
        }

        return prefab;
    }

    static void ProcessWorldMatrix(const std::vector<PrefabAssetNode>& nodes, size_t index, std::vector<Matrix4> &worldMatrices)
    {
        auto& node = nodes[index];
        if (node.parentIndex == ~(0U)) {
            worldMatrices[index] = node.transform;
        } else {
            auto &parent = worldMatrices[node.parentIndex];
            worldMatrices[index] = parent * node.transform;
        }
        for (auto &child : node.children) {
            ProcessWorldMatrix(nodes, child, worldMatrices);
        }
    }

    void Prefab::LoadToScene(RenderScene &scene)
    {
        auto        pass        = std::make_shared<Pass>();
        SubPassInfo subPassInfo = {};
        subPassInfo.colors.emplace_back(AttachmentInfo{VK_FORMAT_B8G8R8A8_UNORM, VK_SAMPLE_COUNT_4_BIT});
        subPassInfo.depthStencil = AttachmentInfo{VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_4_BIT};
        pass->AddSubPass(subPassInfo);
        pass->InitRHI();

        std::vector<Matrix4> worldMatrices(nodes.size());
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (nodes[i].parentIndex == ~(0U)) {
                ProcessWorldMatrix(nodes, i, worldMatrices);
            }
        }

        auto *smFeature  = scene.GetFeature<StaticMeshFeature>();
        for (size_t i = 0; i < nodes.size(); ++i) {
            auto& node = nodes[i];
            if (node.meshIndex != ~(0u)) {
                StaticMesh *staticMesh = smFeature->Create();
                RDMeshPtr mesh = meshes[node.meshIndex]->CreateInstance();

                auto &mat = mesh->GetSubMesh(0).material;
                mat->GetGraphicTechniques()[0]->SetRenderPass(pass); // TODO
                staticMesh->SetMesh(mesh);
                staticMesh->SetWorldMatrix(worldMatrices[i]);
            }
        }
    }
}
