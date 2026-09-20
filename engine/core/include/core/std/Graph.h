//
// Created by blues on 2024/6/2.
//

#pragma once

#include <cstdint>
#include <functional>
#include <queue>
#include <utility>
#include <vector>

namespace sky {

    // Minimal directed graph for dependency ordering.
    // An edge is added as (from -> to), meaning "from depends on to"; TopologicalSort() returns a
    // deterministic order in which every dependency precedes its dependents.
    class Graph {
    public:
        using Vertex = uint32_t;
        using Edge = std::pair<Vertex, Vertex>;

        Vertex AddVertex()
        {
            adjacency.emplace_back();
            inDegree.push_back(0);
            return static_cast<Vertex>(adjacency.size() - 1);
        }

        void AddEdge(Vertex from, Vertex to)
        {
            if (from >= VertexCount() || to >= VertexCount()) {
                return;
            }
            for (const auto &edge : edges) {
                if (edge.first == from && edge.second == to) {
                    return;
                }
            }
            edges.emplace_back(from, to);
            adjacency[to].push_back(from);
            ++inDegree[from];
        }

        uint32_t VertexCount() const
        {
            return static_cast<uint32_t>(adjacency.size());
        }

        const std::vector<Edge> &Edges() const
        {
            return edges;
        }

        bool IsCyclic() const
        {
            return cyclic;
        }

        std::vector<Vertex> TopologicalSort()
        {
            cyclic = false;
            std::vector<uint32_t> degree = inDegree;
            std::priority_queue<Vertex, std::vector<Vertex>, std::greater<>> ready;
            const auto count = VertexCount();
            for (Vertex vtx = 0; vtx < count; ++vtx) {
                if (degree[vtx] == 0) {
                    ready.push(vtx);
                }
            }

            std::vector<Vertex> order;
            order.reserve(count);
            while (!ready.empty()) {
                const Vertex vtx = ready.top();
                ready.pop();
                order.push_back(vtx);
                for (const Vertex next : adjacency[vtx]) {
                    if (--degree[next] == 0) {
                        ready.push(next);
                    }
                }
            }

            if (order.size() != count) {
                cyclic = true;
                for (Vertex vtx = 0; vtx < count; ++vtx) {
                    if (degree[vtx] != 0) {
                        order.push_back(vtx);
                    }
                }
            }
            return order;
        }

    private:
        std::vector<std::vector<Vertex>> adjacency;
        std::vector<Edge>                edges;
        std::vector<uint32_t>            inDegree;
        bool                             cyclic = false;
    };

} // namespace sky
