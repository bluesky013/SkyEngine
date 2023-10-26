//
// Created by Zach Lee on 2023/2/26.
//

#include <render/adaptor/assets/RenderPrefab.h>

namespace sky {
    void RenderPrefabAssetData::Load(BinaryInputArchive &archive)
    {
        uint32_t size = 0;
        archive.LoadValue(size);
        images.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            std::string idStr;
            archive.LoadValue(idStr);
            images[i] = AssetManager::Get()->LoadAsset<Texture>(Uuid::CreateFromString(idStr));
        }

        size = 0;
        archive.LoadValue(size);
        meshes.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            std::string idStr;
            archive.LoadValue(idStr);
            meshes[i] = AssetManager::Get()->LoadAsset<Mesh>(Uuid::CreateFromString(idStr));
        }

        size = 0;
        archive.LoadValue(size);
        materials.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            std::string idStr;
            archive.LoadValue(idStr);
            materials[i] = AssetManager::Get()->LoadAsset<MaterialInstance>(Uuid::CreateFromString(idStr));
        }

        size = 0;
        archive.LoadValue(size);
        nodes.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            auto &node =  nodes[i];
            archive.LoadValue(node.meshIndex);
            archive.LoadValue(node.parentIndex);
            for (float &j : node.localMatrix.v) {
                archive.LoadValue(j);
            }
        }
    }

    void RenderPrefabAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(static_cast<uint32_t>(images.size()));
        for (const auto &image : images) {
            archive.SaveValue(image->GetUuid());
        }

        archive.SaveValue(static_cast<uint32_t>(meshes.size()));
        for (const auto &mesh : meshes) {
            archive.SaveValue(mesh->GetUuid());
        }

        archive.SaveValue(static_cast<uint32_t>(materials.size()));
        for (const auto &material : materials) {
            archive.SaveValue(material->GetUuid());
        }

        archive.SaveValue(static_cast<uint32_t>(nodes.size()));
        for (const auto &node : nodes) {
            archive.SaveValue(node.meshIndex);
            archive.SaveValue(node.parentIndex);
            for (float i : node.localMatrix.v) {
                archive.SaveValue(i);
            }
        }
    }
} // namespace sky