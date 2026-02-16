//
// Created by blues on 2025/2/16.
//

#pragma once

#include <render/resource/Mesh.h>
#include <render/lod/LodGroup.h>

namespace sky {

    class MeshLodGroup : public RenderResource {
    public:
        MeshLodGroup() = default;
        ~MeshLodGroup() override = default;

        void AddLodMesh(const RDMeshPtr &mesh, float screenSize);
        void SetLodBias(float bias);

        uint32_t GetLodCount() const { return static_cast<uint32_t>(lodMeshes.size()); }
        const RDMeshPtr &GetMesh(uint32_t lod) const { return lodMeshes[lod]; }

        uint32_t SelectLod(const AABB &worldBound, const Vector3 &viewPos, float fov, float screenHeight) const;

        const LodConfig &GetConfig() const { return config; }

    private:
        LodConfig config;
        std::vector<RDMeshPtr> lodMeshes;
    };

    using RDMeshLodGroupPtr = CounterPtr<MeshLodGroup>;

} // namespace sky
