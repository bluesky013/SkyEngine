//
// Created by Copilot on 2026/2/16.
//

#pragma once

#include <cstdint>
#include <vector>
#include <core/shapes/AABB.h>
#include <core/template/ReferenceObject.h>
#include <render/resource/Mesh.h>

namespace sky {

    struct HLODNode {
        uint32_t lodLevel   = 0;
        float    switchInDistance  = 0.f;
        float    switchOutDistance = 0.f;

        AABB worldBound;

        RDMeshPtr mesh;

        uint32_t parentIndex = ~0U;
        std::vector<uint32_t> childIndices;
    };

} // namespace sky
