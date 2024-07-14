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
            archive.LoadValue(reinterpret_cast<char*>(&node.mesh), sizeof(Uuid));
            archive.LoadValue(node.parentIndex);
            archive.LoadValue(reinterpret_cast<char*>(&node.localTransform), sizeof(Transform));
        }
    }

    void RenderPrefabAssetData::SaveBin(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(static_cast<uint32_t>(nodes.size()));
        for (const auto &node : nodes) {
            archive.SaveValue(reinterpret_cast<const char*>(&node.mesh), sizeof(Uuid));
            archive.SaveValue(node.parentIndex);
            archive.SaveValue(reinterpret_cast<const char*>(&node.localTransform), sizeof(Transform));
        }
    }

    void RenderPrefabAssetData::LoadJson(JsonInputArchive &archive)
    {


    }

    void RenderPrefabAssetData::SaveJson(JsonOutputArchive &archive) const
    {
        archive.StartObject();

        archive.Key("nodes");
        archive.StartArray();

        for (const auto &node : nodes) {
            archive.StartObject();

            archive.Key("translation");
            archive.SaveValueObject(node.localTransform.translation);

            archive.Key("rotation");
            archive.SaveValueObject(node.localTransform.rotation);

            archive.Key("scale");
            archive.SaveValueObject(node.localTransform.scale);

            archive.Key("mesh");
            archive.SaveValue(node.mesh.ToString());

            archive.Key("parent");
            archive.SaveValue(node.parentIndex);

            archive.EndObject();
        }

        archive.EndArray();

        archive.EndObject();
    }
} // namespace sky