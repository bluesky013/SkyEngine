//
// Mesh asset writer: maps cooked data onto the MeshAssetData / SkinAssetData
// payloads (mirrors aurora/cook/image/ImageAssetWriter.h).
//

#pragma once

#include <aurora/adaptor/assets/SkeletonAsset.h>
#include <aurora/cook/mesh/MeshProcess.h>

namespace sky::aurora::cook {

    void WriteMeshAsset(const CookedMesh &mesh, const Uuid &skin, const Uuid &skeleton, MeshAssetData &out);
    void WriteSkinAsset(const SkinAssetData &skin, SkinAssetData &out);
    void WriteSkeletonAsset(const CookMeshSource &source, SkeletonAssetData &out);

} // namespace sky::aurora::cook
