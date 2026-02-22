//
// Created by blues on 2024/10/11.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <render/debug/DebugRenderer.h>
#include <DebugDraw.h>

class dtNavMesh;

namespace sky::ai {
    void RecastDrawNavMeshPolys(const dtNavMesh& naviMesh, DebugRenderer& debugDraw);

} // namespace sky
