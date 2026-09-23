//
// Created on 2026/09/22.
//

#include <navigation/terrain/TerrainGeometryProvider.h>

#include <terrain/TerrainAddress.h>
#include <terrain/TerrainRegion.h>
#include <terrain/TerrainSystemInterface.h>

#include <vector>

namespace sky::ai {

    namespace {

        // Accumulates LOD0 terrain quads as world-space triangles, clipped to the requested bounds.
        class RegionGeometryAccumulator : public terrain::ITerrainRegionSink {
        public:
            explicit RegionGeometryAccumulator(const AABB &inBounds) : bounds(inBounds) {}

            void OnTerrainTileLod0(const terrain::TerrainTileCoord &coord, const terrain::TerrainMeta &meta,
                                   const float *heights, uint32_t vertexSize) override
            {
                const Vector3  origin = terrain::TileToWorld(meta, coord);
                const float    res    = meta.resolution;
                const uint32_t quads  = vertexSize - 1;

                auto sample = [&](uint32_t x, uint32_t z) {
                    return heights[static_cast<size_t>(z) * vertexSize + x];
                };

                for (uint32_t iz = 0; iz < quads; ++iz) {
                    for (uint32_t ix = 0; ix < quads; ++ix) {
                        const float x0 = origin.x + static_cast<float>(ix) * res;
                        const float x1 = origin.x + static_cast<float>(ix + 1) * res;
                        const float z0 = origin.z + static_cast<float>(iz) * res;
                        const float z1 = origin.z + static_cast<float>(iz + 1) * res;

                        if (x1 < bounds.min.x || x0 > bounds.max.x || z1 < bounds.min.z || z0 > bounds.max.z) {
                            continue;
                        }

                        const uint32_t base = static_cast<uint32_t>(vertices.size());
                        vertices.emplace_back(x0, sample(ix, iz), z0);
                        vertices.emplace_back(x1, sample(ix + 1, iz), z0);
                        vertices.emplace_back(x0, sample(ix, iz + 1), z1);
                        vertices.emplace_back(x1, sample(ix + 1, iz + 1), z1);

                        // Upward-facing winding (+Y normal).
                        indices.push_back(base + 0);
                        indices.push_back(base + 2);
                        indices.push_back(base + 1);
                        indices.push_back(base + 1);
                        indices.push_back(base + 2);
                        indices.push_back(base + 3);
                    }
                }
            }

            const std::vector<Vector3>  &Vertices() const { return vertices; }
            const std::vector<uint32_t> &Indices() const { return indices; }

        private:
            AABB                 bounds;
            std::vector<Vector3>  vertices;
            std::vector<uint32_t> indices;
        };

    } // namespace

    bool TerrainGeometryProvider::Collect(const AABB &bounds, INaviGeometrySink &sink)
    {
        if (system == nullptr) {
            return false;
        }

        RegionGeometryAccumulator accumulator(bounds);
        const bool complete = system->SampleRegionLod0(bounds, accumulator);

        if (!accumulator.Vertices().empty() && !accumulator.Indices().empty()) {
            sink.AddTriangles(accumulator.Vertices().data(), static_cast<uint32_t>(accumulator.Vertices().size()),
                              accumulator.Indices().data(), static_cast<uint32_t>(accumulator.Indices().size()));
        }

        return complete;
    }

} // namespace sky::ai
