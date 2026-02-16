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

    uint32_t MeshLodGroup::SelectLod(const AABB &worldBound, const Vector3 &viewPos, float fov, float screenHeight) const
    {
        float size = CalculateScreenSize(worldBound, viewPos, fov, screenHeight);
        return SelectLodLevel(config, size);
    }

} // namespace sky
