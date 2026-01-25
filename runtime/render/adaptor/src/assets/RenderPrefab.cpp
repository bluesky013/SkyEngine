//
// Created by Zach Lee on 2023/2/26.
//

#include <render/adaptor/assets/RenderPrefab.h>

namespace sky {
    void RenderPrefabAssetData::LoadBin(BinaryInputArchive &archive)
    {
        uint32_t size = 0;
        archive.LoadValue(size);
        nodes.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            auto &node =  nodes[i];
            archive.LoadValue(node.name);
            archive.LoadValue(reinterpret_cast<char*>(&node.mesh), sizeof(Uuid));
            archive.LoadValue(reinterpret_cast<char*>(&node.material), sizeof(Uuid));
            archive.LoadValue(node.parentIndex);
            archive.LoadValue(reinterpret_cast<char*>(&node.localTransform), sizeof(Transform));
        }
    }

    void RenderPrefabAssetData::SaveBin(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(static_cast<uint32_t>(nodes.size()));
        for (const auto &node : nodes) {
            archive.SaveValue(node.name);
            archive.SaveValue(reinterpret_cast<const char*>(&node.mesh), sizeof(Uuid));
            archive.SaveValue(reinterpret_cast<const char*>(&node.material), sizeof(Uuid));
            archive.SaveValue(node.parentIndex);
            archive.SaveValue(reinterpret_cast<const char*>(&node.localTransform), sizeof(Transform));
        }
    }

    void RenderPrefabAssetData::LoadJson(JsonInputArchive &archive)
    {
        uint32_t num = archive.StartArray("nodes");

        nodes.resize(num);
        for (uint32_t i = 0; i < num; ++i) {
            auto &node = nodes[i];

            if (archive.Start("name")) {
                node.name = archive.LoadString();
                archive.End();
            }

            if (archive.Start("translation")) {
                archive.LoadValueObject(node.localTransform.translation);
                archive.End();
            }

            if (archive.Start("rotation")) {
                archive.LoadValueObject(node.localTransform.rotation);
                archive.End();
            }

            if (archive.Start("scale")) {
                archive.LoadValueObject(node.localTransform.scale);
                archive.End();
            }

            if (archive.Start("mesh")) {
                std::string meshId = archive.LoadString();
                node.mesh = Uuid::CreateFromString(meshId);
                archive.End();
            }

            if (archive.Start("material")) {
                std::string materialId = archive.LoadString();
                node.material = Uuid::CreateFromString(materialId);
                archive.End();
            }

            if (archive.Start("parent")) {
                node.parentIndex = archive.LoadUint();
                archive.End();
            }

            archive.NextArrayElement();
        }

        archive.End();
    }

    void RenderPrefabAssetData::SaveJson(JsonOutputArchive &archive) const
    {
        archive.StartObject();

        archive.Key("nodes");
        archive.StartArray();

        for (const auto &node : nodes) {
            archive.StartObject();

            archive.Key("name");
            archive.SaveValue(node.name);

            archive.Key("translation");
            archive.SaveValueObject(node.localTransform.translation);

            archive.Key("rotation");
            archive.SaveValueObject(node.localTransform.rotation);

            archive.Key("scale");
            archive.SaveValueObject(node.localTransform.scale);

            archive.Key("mesh");
            archive.SaveValue(node.mesh.ToString());

            archive.Key("material");
            archive.SaveValue(node.material.ToString());

            archive.Key("parent");
            archive.SaveValue(node.parentIndex);

            archive.EndObject();
        }

        archive.EndArray();

        archive.EndObject();
    }
} // namespace sky