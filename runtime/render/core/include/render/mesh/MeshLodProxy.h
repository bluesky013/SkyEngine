//
// Created by blues on 2026/2/17.
//

#pragma once

#include <boost/graph/graph_concepts.hpp>
#include <render/lod/LodTypes.h>
#include <render/resource/Mesh.h>
#include <render/resource/SkeletonMesh.h>

namespace sky {

    class MeshLodProxy : public LodProxy {
    public:
        MeshLodProxy(const RDMeshPtr& inMesh, const LodLevel &inLevel);
        ~MeshLodProxy() override = default;

        const RDMeshPtr& GetMesh() const { return mesh; }

        uint32_t GetSectionNum() const noexcept override { return static_cast<uint32_t>(mesh->GetSubMeshes().size()); }
    private:
        bool IsValid() const noexcept override { return !!mesh; }

        void Reset() noexcept override;

        RDMeshPtr mesh;
    };

    class SkeletalMeshLodProxy : public LodProxy {
    public:
        SkeletalMeshLodProxy(const RDSkeletonMeshPtr& inMesh, const LodLevel& level);
        ~SkeletalMeshLodProxy() override = default;

        const RDSkeletonMeshPtr& GetMesh() const { return mesh; }

        uint32_t GetSectionNum() const noexcept override { return static_cast<uint32_t>(mesh->GetSubMeshes().size()); }

        bool HasSkin() const noexcept override { return true; }
    private:
        bool IsValid() const noexcept override { return !!mesh; }

        void Reset() noexcept override;

        RDSkeletonMeshPtr mesh;
    };

} // namespace sky
