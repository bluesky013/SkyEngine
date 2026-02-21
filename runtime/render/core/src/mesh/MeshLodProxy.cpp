//
// Created by blues on 2026/2/17.
//

#include <render/mesh/MeshLodProxy.h>

namespace sky {

    MeshLodProxy::MeshLodProxy(const RDMeshPtr& inMesh, const LodLevel &inLevel)
        : LodProxy(inLevel)
        , mesh(inMesh)
    {
    }

    void MeshLodProxy::Reset()
    {
        mesh = nullptr;
    }

} // namespace sky