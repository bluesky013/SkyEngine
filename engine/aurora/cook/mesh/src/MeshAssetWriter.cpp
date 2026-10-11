//
// Mesh asset writer (see MeshAssetWriter.h).
//

#include <aurora/cook/mesh/MeshAssetWriter.h>

namespace sky::aurora::cook {

    void WriteMeshAsset(const CookedMesh &mesh, const CookSkin &skin, const Uuid &skeleton, MeshAssetData &out)
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

        // Skin is absorbed into the mesh: fixed-size arrays go to the bulk,
        // bone names stay in the (resident) header.
        out.skinBoneNames    = skin.boneNames;
        out.skinBoneCount    = static_cast<uint32_t>(skin.inverseBindMatrices.size());
        out.skinMappingCount = static_cast<uint32_t>(skin.boneMapping.size());
        if (!skin.inverseBindMatrices.empty()) {
            const auto *bytes = reinterpret_cast<const uint8_t *>(skin.inverseBindMatrices.data());
            out.skinData.insert(out.skinData.end(), bytes, bytes + skin.inverseBindMatrices.size() * sizeof(Matrix4));
        }
        if (!skin.boneMapping.empty()) {
            const auto *bytes = reinterpret_cast<const uint8_t *>(skin.boneMapping.data());
            out.skinData.insert(out.skinData.end(), bytes, bytes + skin.boneMapping.size() * sizeof(uint32_t));
        }

        // Section layout within the product bulk: vertex -> index -> skin.
        out.vertexOffset = 0;
        out.vertexSize   = out.vertexData.size();
        out.indexOffset  = out.vertexSize;
        out.indexSize    = out.indexData.size();
        out.skinOffset   = out.indexOffset + out.indexSize;
        out.skinSize     = out.skinData.size();

        out.skeleton = skeleton;
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
