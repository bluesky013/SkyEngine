//
// Mesh asset writer: maps cooked data onto the MeshAssetData / SkinAssetData
// payloads (mirrors aurora/cook/image/ImageAssetWriter.h).
//

#pragma once

#include <aurora/cook/mesh/MeshProcess.h>

namespace sky::aurora::cook {

    void WriteMeshAsset(const CookedMesh &mesh, const Uuid &skin, MeshAssetData &out);
    void WriteSkinAsset(const SkinAssetData &skin, SkinAssetData &out);

} // namespace sky::aurora::cook
