//
// Mesh asset writer: maps cooked data onto the MeshAssetData / SkeletonAssetData
// payloads (mirrors aurora/cook/image/ImageAssetWriter.h).
//

#pragma once

#include <aurora/adaptor/assets/SkeletonAsset.h>
#include <aurora/cook/mesh/MeshProcess.h>
#include <span>

namespace sky::aurora::cook {

    MeshCookStatus BindMeshMaterials(Asset<Mesh> &asset, std::span<const Uuid> materials);
    void           WriteMeshAsset(const CookedMesh &mesh, MeshAssetData &out);
    void           WriteSkeletonAsset(const CookMeshSource &source, SkeletonAssetData &out);

} // namespace sky::aurora::cook
