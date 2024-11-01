//
// Created by blues on 2024/10/11.
//

#include <recast/RecastDebugDraw.h>
#include <DetourNavMesh.h>

namespace sky::ai {

    void RecastDebugDraw::depthMask(bool state)
    {
    }
    void RecastDebugDraw::texture(bool state)
    {
    }
    void RecastDebugDraw::begin(duDebugDrawPrimitives prim, float size)
    {
    }
    void RecastDebugDraw::vertex(const float* pos, unsigned int color)
    {
    }
    void RecastDebugDraw::vertex(const float x, const float y, const float z, unsigned int color)
    {
    }
    void RecastDebugDraw::vertex(const float* pos, unsigned int color, const float* uv)
    {
    }
    void RecastDebugDraw::vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v)
    {
    }
    void RecastDebugDraw::end()
    {
    }

    static void DrawNavMeshPoly(const dtNavMesh& mesh, dtPolyRef ref, const uint32_t color, RecastDebugDraw &dd)
    {
        const dtMeshTile* tile = nullptr;
        const dtPoly* poly = nullptr;
        if (dtStatusFailed(mesh.getTileAndPolyByRef(ref, &tile, &poly))) {
            return;
        }
        const auto c = duTransCol(color, 64);
        const auto ip = (unsigned int)(poly - tile->polys);

        const dtPolyDetail* pd = &tile->detailMeshes[ip];
        for (int i = 0; i < pd->triCount; ++i)
        {
            const auto* t = &tile->detailTris[(pd->triBase+i)*4];

            dd.begin(duDebugDrawPrimitives::DU_DRAW_TRIS);
            for (int j = 0; j < 3; ++j)
            {
                auto vtx = t[j] < poly->vertCount ?
                    tile->verts[poly->verts[t[j]]*3] :
                    tile->detailVerts[(pd->vertBase+t[j]-poly->vertCount)*3];

                dd.vertex(&vtx, c);
            }
            dd.end();
        }
    }

    void RecastDrawNavMeshPolys(const dtNavMesh& mesh, RecastDebugDraw& debugDraw)
    {
        for (int i = 0; i < mesh.getMaxTiles(); ++i) {
            const dtMeshTile* tile = mesh.getTile(i);
            if (tile->header == nullptr) {
                continue;
            }
            dtPolyRef base = mesh.getPolyRefBase(tile);

            for (int j = 0; j < tile->header->polyCount; ++j) {
                DrawNavMeshPoly(mesh, base | (dtPolyRef)j, duRGBA(0,0,0,128), debugDraw);
            }
        }
    }
} // namespace sky::ai