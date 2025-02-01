//
// Created by blues on 2025/1/1.
//

#pragma once

#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <vector>
#include <render/RenderResource.h>
#include <render/RenderGeometry.h>
#include <render/resource/Material.h>

namespace sky {

    struct Meshlet {
        uint32_t vertexOffset;
        uint32_t triangleOffset;
        uint32_t vertexCount;
        uint32_t triangleCount;
    };

    struct MeshletBound {
        Vector4 center; // 0-2: center 3: radius
        Vector4 coneApex;
        Vector4 coneAxis;
    };

    struct MeshletData {
        // components
        std::vector<Meshlet>      meshlets;
        std::vector<MeshletBound> meshletBounds;
        std::vector<RDMaterialInstancePtr> materials;

        // geometry
        std::vector<uint32_t>     meshletVertices;
        std::vector<uint8_t>      meshletTriangles;
    };

    class ClusterMesh : public RenderResource {
    public:
        ClusterMesh() = default;
        ~ClusterMesh() override = default;

    private:
        // desc
        MeshletData       data;
        RenderGeometryPtr geometry;
    };

} // namespace sky