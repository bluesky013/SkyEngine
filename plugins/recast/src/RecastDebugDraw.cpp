//
// Created by blues on 2024/10/11.
//

#include <recast/RecastDebugDraw.h>

#include <DebugDraw.h>
#include <DetourNavMesh.h>

namespace sky::ai {

    namespace {
        Vector4 ToColor(uint32_t color)
        {
            return Vector4(
                static_cast<float>(color & 0xff) / 255.f,
                static_cast<float>((color >> 8) & 0xff) / 255.f,
                static_cast<float>((color >> 16) & 0xff) / 255.f,
                static_cast<float>((color >> 24) & 0xff) / 255.f);
        }

        void AppendNavMeshPoly(const dtNavMesh &mesh, dtPolyRef ref, uint32_t color, NaviDebugGeometry &out)
        {
            const dtMeshTile *tile = nullptr;
            const dtPoly     *poly = nullptr;
            if (dtStatusFailed(mesh.getTileAndPolyByRef(ref, &tile, &poly))) {
                return;
            }

            const auto vertexColor = ToColor(color);
            const auto ip          = static_cast<unsigned int>(poly - tile->polys);
            const dtPolyDetail *pd = &tile->detailMeshes[ip];

            for (int i = 0; i < pd->triCount; ++i) {
                const auto *t = &tile->detailTris[(pd->triBase + i) * 4];
                for (int j = 0; j < 3; ++j) {
                    const auto *vtx = t[j] < poly->vertCount
                        ? &tile->verts[poly->verts[t[j]] * 3]
                        : &tile->detailVerts[(pd->vertBase + t[j] - poly->vertCount) * 3];

                    out.vertices.push_back(NaviDebugVertex{Vector3(vtx[0], vtx[1], vtx[2]), vertexColor});
                }
            }
        }
    } // namespace

    void RecastBuildNavMeshGeometry(const dtNavMesh &navMesh, NaviDebugGeometry &out)
    {
        out.vertices.clear();

        const uint32_t color = duTransCol(duRGBA(0, 32, 0, 128), 64);

        for (int i = 0; i < navMesh.getMaxTiles(); ++i) {
            const dtMeshTile *tile = navMesh.getTile(i);
            if (tile->header == nullptr) {
                continue;
            }

            const dtPolyRef base = navMesh.getPolyRefBase(tile);
            for (int j = 0; j < tile->header->polyCount; ++j) {
                AppendNavMeshPoly(navMesh, base | static_cast<dtPolyRef>(j), color, out);
            }
        }
    }

} // namespace sky::ai
