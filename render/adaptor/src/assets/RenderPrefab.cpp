//
// Created by Zach Lee on 2023/2/26.
//

#include <render/adaptor/assets/RenderPrefab.h>

namespace sky {
    void RenderPrefabAssetData::Load(BinaryInputArchive &archive)
    {
        uint32_t size = 0;
        archive.LoadValue(size);
        nodes.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            auto &node =  nodes[i];
            archive.LoadValue(reinterpret_cast<char*>(&node.mesh), sizeof(Uuid));
            archive.LoadValue(node.parentIndex);
            archive.LoadValue(reinterpret_cast<char*>(&node.localMatrix), sizeof(Matrix4));
        }
    }

    void RenderPrefabAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(static_cast<uint32_t>(nodes.size()));
        for (const auto &node : nodes) {
            archive.SaveValue(reinterpret_cast<const char*>(&node.mesh), sizeof(Uuid));
            archive.SaveValue(node.parentIndex);
            archive.SaveValue(reinterpret_cast<const char*>(&node.localMatrix), sizeof(Matrix4));
        }
    }
} // namespace sky