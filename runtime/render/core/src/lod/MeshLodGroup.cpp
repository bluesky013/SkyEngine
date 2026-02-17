//
// Created by blues on 2025/2/16.
//

#include <render/lod/MeshLodGroup.h>

namespace sky {

    void MeshLodGroup::AddLodMesh(const RDMeshPtr &mesh, float screenSize)
    {
        lodMeshes.emplace_back(mesh);
        config.lodLevels.emplace_back(LodLevel{screenSize});
    }

    void MeshLodGroup::SetLodBias(float bias)
    {
        config.lodBias = bias;
    }

    void MeshLodGroup::PreComputeDistances(float sphereRadius, const Matrix4 &projMatrix)
    {
        sky::PreComputeDistances(config, sphereRadius, projMatrix);
    }

    uint32_t MeshLodGroup::SelectLod(const AABB &worldBound, const Vector3 &viewPos) const
    {
        return SelectLodLevel(config, worldBound, viewPos);
    }

} // namespace sky
