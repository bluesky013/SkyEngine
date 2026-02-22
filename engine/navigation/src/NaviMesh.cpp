//
// Created by blues on 2024/9/1.
//

#include <navigation/NaviMesh.h>

namespace sky::ai {

    void NaviMesh::PrepareForBuild()
    {
        const auto &ext = buildBounds.max - buildBounds.min;
        auto maxExt = std::max(std::max(ext.x, ext.y), ext.z);
        octree = std::make_unique<NaviOctree>(maxExt);
    }


} // namespace sky::ai