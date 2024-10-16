//
// Created by blues on 2024/10/14.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/shapes/AABB.h>
#include <rhi/Core.h>
#include <vector>

namespace sky {

    struct TriangleMeshView {
        uint32_t numVert;
        uint32_t numTris;
        const uint8_t *triBase = nullptr;
        const uint8_t *vertexBase = nullptr;

        AABB aabb;
    };

    struct TriangleMesh : public RefObject {
        void AddView(uint32_t firstVtx, uint32_t numVtx, uint32_t firstIdx, uint32_t numIdx, const AABB &box);

        std::vector<uint8_t> indexRaw;
        std::vector<uint8_t> position;
        std::vector<TriangleMeshView> views;

        uint32_t vtxStride = sizeof(Vector3);
        rhi::IndexType indexType = rhi::IndexType::U32;
    };
} // namespace sky

