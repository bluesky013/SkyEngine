//
// Created by blues on 2025/2/16.
//

#include <render/lod/MeshLodGroup.h>

namespace sky {

    void MeshLodGroup::AddLodMesh(const RDMeshPtr &mesh, float screenSize)
    {
        lodMeshes.emplace_back(mesh);
        config.lodLevels.emplace_back(LodLevel{screenSize, 0.f});
    }

    void MeshLodGroup::AddLodMesh(const RDMeshPtr &mesh, float screenSize, float distance)
    {
        lodMeshes.emplace_back(mesh);
        config.lodLevels.emplace_back(LodLevel{screenSize, distance});
    }

    void MeshLodGroup::SetLodBias(float bias)
    {
        config.lodBias = bias;
    }

    void MeshLodGroup::SetLodPolicy(LodPolicy policy)
    {
        config.policy = policy;
    }

    uint32_t MeshLodGroup::SelectLod(const AABB &worldBound, const Vector3 &viewPos, float fov) const
    {
        return SelectLodLevel(config, worldBound, viewPos, fov);
    }

} // namespace sky
