//
// Mesh asset writer: maps cooked data onto the MeshAssetData / SkeletonAssetData
// payloads (mirrors aurora/cook/image/ImageAssetWriter.h).
//

#pragma once

#include <aurora/adaptor/assets/SkeletonAsset.h>
#include <aurora/cook/mesh/MeshProcess.h>

namespace sky::aurora::cook {

    void WriteMeshAsset(const CookedMesh &mesh, const CookSkin &skin, const Uuid &skeleton, MeshAssetData &out);
    void WriteSkeletonAsset(const CookMeshSource &source, SkeletonAssetData &out);

} // namespace sky::aurora::cook
