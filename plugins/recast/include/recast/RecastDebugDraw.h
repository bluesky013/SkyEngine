//
// Created by blues on 2024/10/11.
//

#pragma once

#include <navigation/NaviDebugGeometry.h>

class dtNavMesh;

namespace sky::ai {
    void RecastBuildNavMeshGeometry(const dtNavMesh &navMesh, NaviDebugGeometry &out);

} // namespace sky::ai
