//
// Created by Copilot on 2026/2/16.
//

#include <render/hlod/HLODTree.h>
#include <algorithm>
#include <cmath>

namespace sky {

    uint32_t HLODTree::AddNode(const HLODNode &node)
    {
        uint32_t index = static_cast<uint32_t>(nodes.size());
        nodes.emplace_back(node);
        return index;
    }

    const HLODNode &HLODTree::GetNode(uint32_t index) const
    {
        return nodes[index];
    }

    void HLODTree::SelectLODs(const Vector3 &cameraPos, std::vector<uint32_t> &visibleNodes) const
    {
        if (nodes.empty()) {
            return;
        }

        for (uint32_t i = 0; i < static_cast<uint32_t>(nodes.size()); ++i) {
            if (nodes[i].parentIndex == ~0U) {
                SelectLODRecursive(i, cameraPos, visibleNodes);
            }
        }
    }

    void HLODTree::SelectLODRecursive(uint32_t nodeIndex, const Vector3 &cameraPos, std::vector<uint32_t> &visibleNodes) const
    {
        const auto &node = nodes[nodeIndex];
        float distSq = ComputeDistanceSq(node.worldBound, cameraPos);

        float switchOutSq = node.switchOutDistance * node.switchOutDistance;
        if (switchOutSq > 0.f && distSq >= switchOutSq) {
            return;
        }

        float switchInSq = node.switchInDistance * node.switchInDistance;
        bool useChildren = !node.childIndices.empty() && distSq < switchInSq;

        if (useChildren) {
            for (uint32_t childIdx : node.childIndices) {
                SelectLODRecursive(childIdx, cameraPos, visibleNodes);
            }
        } else if (node.mesh) {
            visibleNodes.emplace_back(nodeIndex);
        }
    }

    float HLODTree::ComputeDistanceSq(const AABB &bound, const Vector3 &point) const
    {
        Vector3 center = {
            (bound.min.x + bound.max.x) * 0.5f,
            (bound.min.y + bound.max.y) * 0.5f,
            (bound.min.z + bound.max.z) * 0.5f
        };
        float dx = point.x - center.x;
        float dy = point.y - center.y;
        float dz = point.z - center.z;
        return dx * dx + dy * dy + dz * dz;
    }

} // namespace sky
