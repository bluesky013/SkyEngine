//
// Created by blues on 2024/10/14.
//

#include <core/shapes/TriangleMesh.h>

namespace sky {

    void TriangleMesh::AddView(uint32_t firstVtx, uint32_t numVtx, uint32_t firstIdx, uint32_t numIdx, const AABB &box)
    {
        views.emplace_back();

        TriangleMeshView &view = views.back();
        view.numVert = numVtx;
        view.numTris = numIdx / 3;

        view.firstVertex = firstVtx;
        view.firstIndex  = firstIdx;

        view.aabb = box;
    }

} // namespace sky