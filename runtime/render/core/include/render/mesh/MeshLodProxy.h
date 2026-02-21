//
// Created by blues on 2026/2/17.
//

#pragma once

#include <render/lod/LodTypes.h>
#include <render/resource/Mesh.h>

namespace sky {

    class MeshLodProxy : public LodProxy {
    public:
        MeshLodProxy(const RDMeshPtr& inMesh, const LodLevel &inLevel);
        ~MeshLodProxy() override = default;

        const RDMeshPtr& GetMesh() const { return mesh; }
    private:
        bool IsValid() const override { return !!mesh; }

        void Reset() override;

        RDMeshPtr mesh;
    };

} // namespace sky
