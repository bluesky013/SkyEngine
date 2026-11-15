//
// Mesh asset writer (see MeshAssetWriter.h).
//

#include <aurora/cook/mesh/MeshAssetWriter.h>

#include <algorithm>

namespace sky::aurora::cook {

    MeshCookStatus BindMeshMaterials(Asset<Mesh> &asset, std::span<const Uuid> materials)
    {
        std::vector<Uuid> slots(materials.begin(), materials.end());
        uint32_t          count = 0;
        for (const auto &part : asset.Data().subMeshes) {
            if (part.materialIndex >= 8192)
                return {MeshCookError::InvalidData, "Mesh material slot exceeds the product limit"};
            count = std::max(count, part.materialIndex + 1);
        }
        if (slots.size() > 8192 || slots.size() < count || std::any_of(slots.begin(), slots.end(), [](const Uuid &id) { return !id; }))
            return {MeshCookError::InvalidData, "Mesh material slots are incomplete or exceed the product limit"};
        std::vector<Uuid> dependencies;
        for (const auto &id : slots)
            if (std::find(dependencies.begin(), dependencies.end(), id) == dependencies.end())
                dependencies.push_back(id);
        asset.Data().materials = std::move(slots);
        for (const auto &id : dependencies)
            asset.AddDependencies(id);
        return {};
    }

    void WriteMeshAsset(const CookedMesh &mesh, MeshAssetData &out)
    {
        out.clear();
        out.version = MeshAssetData::CURRENT_VERSION;

        out.vertexStride = mesh.vertexStride;
        out.vertexCount  = mesh.vertexCount;
        out.indexCount   = mesh.indexCount;
        out.indexType    = mesh.indexType;
        out.attributes   = mesh.attributes;

        out.vertexData = mesh.vertexData;
        out.indexData  = mesh.indexData;
        out.subMeshes  = mesh.subMeshes;
        out.bounds     = BoundingBoxSphere(mesh.bounds);

        out.skinInput    = mesh.skinInput;
        out.slotBounds   = mesh.slotBounds;
        out.vertexOffset = 0;
        out.vertexSize   = out.vertexData.size();
        out.indexOffset  = out.vertexSize;
        out.indexSize    = out.indexData.size();
    }

    void WriteSkeletonAsset(const CookMeshSource &source, SkeletonAssetData &out)
    {
        out.clear();
        out.version = SkeletonAssetData::CURRENT_VERSION;
        out.boneData.resize(source.bones.size());
        out.refPos.resize(source.bones.size());
        for (size_t i = 0; i < source.bones.size(); ++i) {
            out.boneData[i].name        = Name(source.bones[i].name.c_str());
            out.boneData[i].parentIndex = source.bones[i].parentIndex;
            out.refPos[i]               = source.bones[i].refTransform;
        }
    }

} // namespace sky::aurora::cook
