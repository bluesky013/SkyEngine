//
// Created by blues on 2025/2/9.
//

#include <builder/render/mesh/ClusterBuilder.h>
#include <core/math/MathUtil.h>
#include "MetisInclude.h"

#include <boost/graph/adjacency_list.hpp>

#include <map>

namespace sky::builder {

    struct MeshGraphData {
        idx_t num;

        std::vector<idx_t> adjacency;
        std::vector<idx_t> adjacencyCost;
        std::vector<idx_t> adjacencyOffset;
    };

    static uint32_t HashPosition(const Vector3& pos)
    {
        return Murmur3Hash32({
            UnionFloatToU32(pos.x),
            UnionFloatToU32(pos.y),
            UnionFloatToU32(pos.z),
        }, 0);
    }

    class EdgeHash {
    public:
        EdgeHash(const TBufferViewAccessor<Vector3> &pos, const TBufferViewAccessor<uint32_t> &idx)
            : position(pos)
            , indices(idx)
        {
        }

        void AddEdge(idx_t index)
        {
            // (0 - 1 - 2) -> (1 - 2 - 0)
            // (3 - 4 - 5) -> (4 - 5 - 3)
            idx_t v1 = index;
            idx_t v2 = index - (index % 3) + ((1 << (index % 3)) & 3);

            uint32_t id1 = indices[v1];
            uint32_t id2 = indices[v2];

            Vector3 p1 = position[id1];
            Vector3 p2 = position[id2];

            uint32_t hash = Murmur3Hash32({ HashPosition(p1), HashPosition(p2) }, 0);
            hashMap[hash].emplace_back(index);
        }

        template <typename Func>
        void ForEachEdge(idx_t index, Func &&fn)
        {
            idx_t v1 = index;
            idx_t v2 = index - (index % 3) + ((1 << (index % 3)) & 3);

            uint32_t id1 = indices[v1];
            uint32_t id2 = indices[v2];

            Vector3 p1 = position[id1];
            Vector3 p2 = position[id2];

            uint32_t hash = Murmur3Hash32({ HashPosition(p2), HashPosition(p1) }, 0);

            if (auto iter = hashMap.find(hash); iter != hashMap.end()) {
                for (auto &idx : iter->second) {
                    idx_t tV1 = idx;
                    idx_t tV2 = idx - (idx % 3) + ((1 << (idx % 3)) & 3);

                    uint32_t tId1 = indices[tV2];
                    uint32_t tId2 = indices[tV1];

                    Vector3 tP1 = position[tId1];
                    Vector3 tP2 = position[tId2];

                    if (p1 == tP1 && p2 == tP2) {
                        fn(index, idx);
                    }
                }
            }
        }

    private:
        TBufferViewAccessor<Vector3>  position;
        TBufferViewAccessor<uint32_t> indices;

        std::unordered_map<uint32_t, std::vector<idx_t>> hashMap;
    };

    template <typename Key, typename Value>
    class MultiMap : public std::multimap<Key, Value> {
    public:
        MultiMap()  = default;
        ~MultiMap() = default;

        using KeyType   = Key;
        using ValueType = Value;

        void AddUnique(const KeyType &key, const ValueType &value)
        {
            auto range = this->equal_range(key);
            for (auto i = range.first; i != range.second; ++i) {
                if (i->second == value) {
                    return;
                }
            }

//            this->emplace(key, value)->second;
        }
    };

    struct Adjacency {
        std::vector<idx_t> direct;
        MultiMap<idx_t, idx_t> extended;

        template <typename Func>
        void ForEach(idx_t index, Func&& fn)
        {
            idx_t adjIndex = direct[index];
            if (adjIndex >= 0) {
                fn(index, adjIndex);
            }

            auto range = extended.equal_range(index);
            for (auto i = range.first; i != range.second; ++i) {
                fn(index, i->second);
            }
        }
    };

    struct DisjointSet {
        std::vector<idx_t> parents;
    };

    static void UnionSequential(DisjointSet &set, idx_t x, idx_t y)
    {
        idx_t  px = x;
        idx_t py = set.parents[y];
        while (px != py) {
            set.parents[y] = px;
            if (y == py) {
                return;
            }
            y  = py;
            py = set.parents[y];
        }
    }

    static void ClusterTriangles(const TBufferViewAccessor<Vector3>& positions, const TBufferViewAccessor<uint32_t>& indices,
        std::vector<Meshlet>& meshlets)
    {
        auto nIndex = indices.count;
        idx_t triangles = static_cast<idx_t>(nIndex) / 3;

        EdgeHash edgeHash(positions, indices);

        Adjacency adjacency;
        adjacency.direct.resize(nIndex);

        DisjointSet disjointSet;
        disjointSet.parents.resize(triangles);

        // hash edges
        for (idx_t i = 0; i < static_cast<idx_t>(nIndex); ++i) {
            edgeHash.AddEdge(i);
        }

        // find shared edges
        for (idx_t i = 0; i < static_cast<idx_t>(nIndex); ++i) {
            idx_t adjIndex = -1;
            idx_t adjCount = 0;

            edgeHash.ForEachEdge(i, [&adjIndex, &adjCount](idx_t id1, idx_t id2) {
                adjIndex = id2;
                adjCount++;
            });

            if (adjCount > 1) {
                adjIndex = -2;
            }

            adjacency.direct[i] = adjIndex;
        }

        for (idx_t i = 0; i < static_cast<idx_t>(nIndex); ++i) {
            if (adjacency.direct[i] == -2) {

                std::vector<std::pair<idx_t, idx_t>> edges;
                edgeHash.ForEachEdge(i, [&edges](idx_t id1, idx_t id2) {
                    edges.emplace_back(id1, id2);
                });

                for (const auto &edge : edges) {
                    if (adjacency.direct[edge.first] < 0 && adjacency.direct[edge.second] < 0) {
                        adjacency.direct[edge.first] = edge.second;
                        adjacency.direct[edge.second] = edge.first;
                    } else {
                        adjacency.extended.AddUnique(edge.first, edge.second);
                        adjacency.extended.AddUnique(edge.second, edge.first);
                    }
                }
            }

            adjacency.ForEach(i, [&](idx_t id1, idx_t id2) {

                if (id1 > id2) {
                    UnionSequential(disjointSet, id1 / 3, id2 / 3);
                }

            });
        }
    }

    static void Partition(MeshGraphData &graph, uint32_t minSize, uint32_t maxSize)
    {
        const uint32_t targetSize = ( minSize + maxSize ) / 2;
        const uint32_t targetNum = Ceil( graph.num, targetSize );

        if (targetNum > 1) {
            idx_t options[ METIS_NOPTIONS ];
            METIS_SetDefaultOptions( options );
            options[METIS_OPTION_UFACTOR] = 100;

            idx_t nConstraints = 1;
            idx_t nParts = 2;
            idx_t edgeCut = 0;

            std::vector<idx_t> xadj;
            std::vector<idx_t> adjncy;
            std::vector<idx_t> adjWgt;
            std::vector<idx_t> part;

            int r = METIS_PartGraphRecursive(&graph.num,
                                             &nConstraints,
                                             xadj.data(),
                                             adjncy.data(),
                                             nullptr,
                                             nullptr,
                                             adjWgt.data(),
                                             &nParts,
                                             nullptr,
                                             nullptr,
                                             options,
                                             &edgeCut,
                                             part.data());
            SKY_ASSERT(r == METIS_OK);

        }
    }

    void ClusterBuilder::BuildFromMeshData(const TBufferViewAccessor<Vector3>& position,
                                           const TBufferViewAccessor<uint32_t>& indices)
    {
        std::vector<Meshlet> meshlet;
        ClusterTriangles(position, indices, meshlet);
    }

} // namespace sky::builder
