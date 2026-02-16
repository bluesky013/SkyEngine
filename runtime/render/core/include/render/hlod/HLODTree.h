//
// Created by Copilot on 2026/2/16.
//

#pragma once

#include <vector>
#include <core/math/Vector3.h>
#include <render/hlod/HLODNode.h>

namespace sky {

    class HLODTree : public RefObject {
    public:
        HLODTree() = default;
        ~HLODTree() override = default;

        uint32_t AddNode(const HLODNode &node);
        const HLODNode &GetNode(uint32_t index) const;
        uint32_t GetNodeCount() const { return static_cast<uint32_t>(nodes.size()); }

        void SelectLODs(const Vector3 &cameraPos, std::vector<uint32_t> &visibleNodes) const;

    private:
        void SelectLODRecursive(uint32_t nodeIndex, const Vector3 &cameraPos, std::vector<uint32_t> &visibleNodes) const;
        float ComputeDistanceSq(const AABB &bound, const Vector3 &point) const;

        std::vector<HLODNode> nodes;
    };
    using HLODTreePtr = CounterPtr<HLODTree>;

} // namespace sky
