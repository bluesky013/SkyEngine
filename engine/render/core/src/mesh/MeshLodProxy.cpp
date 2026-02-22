//
// Created by blues on 2026/2/17.
//

#include <render/mesh/MeshLodProxy.h>

namespace sky {

    MeshLodProxy::MeshLodProxy(const RDMeshPtr& inMesh, const LodLevel &inLevel)
        : LodProxy(inLevel)
        , mesh(inMesh)
    {
        SetLocalBounds(mesh->GetGeometry()->localBounds);
    }

    void MeshLodProxy::Reset() noexcept
    {
        mesh = nullptr;
    }

    SkeletalMeshLodProxy::SkeletalMeshLodProxy(const RDSkeletonMeshPtr& inMesh, const LodLevel &inLevel)
        : LodProxy(inLevel)
        , mesh(inMesh)
    {
        SetLocalBounds(mesh->GetGeometry()->localBounds);
    }

    void SkeletalMeshLodProxy::Reset() noexcept
    {
        mesh = nullptr;
    }

} // namespace sky