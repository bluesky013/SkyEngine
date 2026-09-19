//
// Mesh asset writer (see MeshAssetWriter.h).
//

#include <aurora/cook/mesh/MeshAssetWriter.h>

namespace sky::aurora::cook {

    void WriteMeshAsset(const CookedMesh &mesh, const Uuid &skin, MeshAssetData &out)
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
        out.bounds     = mesh.bounds;

        out.meshlets         = mesh.meshlets;
        out.meshletVertices  = mesh.meshletVertices;
        out.meshletTriangles = mesh.meshletTriangles;
        out.meshletBounds    = mesh.meshletBounds;

        out.skin = skin;
    }

    void WriteSkinAsset(const SkinAssetData &skin, SkinAssetData &out)
    {
        out.clear();
        out.version             = SkinAssetData::CURRENT_VERSION;
        out.inverseBindMatrices = skin.inverseBindMatrices;
        out.boneNames           = skin.boneNames;
        out.boneMapping         = skin.boneMapping;
    }

} // namespace sky::aurora::cook
