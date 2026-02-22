//
// Created by blues on 2024/10/11.
//

#include <recast/RecastDebugDraw.h>
#include <DetourNavMesh.h>

namespace sky::ai {
    static void DrawNavMeshPoly(const dtNavMesh& mesh, dtPolyRef ref, const uint32_t color, DebugRenderer &dd)
    {
        const dtMeshTile* tile = nullptr;
        const dtPoly* poly = nullptr;
        if (dtStatusFailed(mesh.getTileAndPolyByRef(ref, &tile, &poly))) {
            return;
        }
        const auto c = duTransCol(color, 64);
        const auto ip = (unsigned int)(poly - tile->polys);

        const dtPolyDetail* pd = &tile->detailMeshes[ip];

        dd.SetColor(Color32(c));
        for (int i = 0; i < pd->triCount; ++i)
        {
            const auto* t = &tile->detailTris[(pd->triBase+i)*4];

            Vector3 triangle[3];
            for (int j = 0; j < 3; ++j)
            {
                auto *vtx = t[j] < poly->vertCount ?
                    &tile->verts[poly->verts[t[j]] * 3] :
                    &tile->detailVerts[(pd->vertBase + t[j]-poly->vertCount) * 3];

                triangle[j].x = vtx[0];
                triangle[j].y = vtx[1];
                triangle[j].z = vtx[2];
            }
            dd.DrawTriangle(triangle[0], triangle[1], triangle[2]);
        }
    }

    void RecastDrawNavMeshPolys(const dtNavMesh& mesh, DebugRenderer& debugDraw)
    {
        for (int i = 0; i < mesh.getMaxTiles(); ++i) {
            const dtMeshTile* tile = mesh.getTile(i);
            if (tile->header == nullptr) {
                continue;
            }
            dtPolyRef base = mesh.getPolyRefBase(tile);

            for (int j = 0; j < tile->header->polyCount; ++j) {
                DrawNavMeshPoly(mesh, base | (dtPolyRef)j, duRGBA(0,32,0,128), debugDraw);
            }
        }
    }
} // namespace sky::ai