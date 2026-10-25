//
// Mesh asset writer (see MeshAssetWriter.h).
//

#include <aurora/cook/mesh/MeshAssetWriter.h>

namespace sky::aurora::cook {

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
