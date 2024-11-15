//
// Created by blues on 2024/11/7.
//

#pragma once

#include <core/math/Vector3.h>
#include <core/shapes/Box2D.h>
#include <terrain/TerrainBase.h>

#include <vector>

namespace sky {

    struct QuadTreeChild {
        uint8_t index;

        static constexpr uint8_t INVALID = 4;
        explicit constexpr QuadTreeChild(uint8_t id) : index(id) {}
        constexpr QuadTreeChild() : index(INVALID) {}
        constexpr QuadTreeChild(uint8_t x, uint8_t y)
        {
            index = (x << 0) | (y << 1);
        }

        TerrainCoord Offset(const TerrainCoord &ori, uint32_t ext) const
        {
            auto ix = static_cast<int>((((index >> 0) & 1) << 1) - 1) * static_cast<int>(ext) + ori.x;
            auto iy = static_cast<int>((((index >> 1) & 1) << 1) - 1) * static_cast<int>(ext) + ori.y;

            return TerrainCoord{ix, iy};
        }
    };

    class TerrainQuadTree {
    public:
        explicit TerrainQuadTree(const TerrainConfig &cfg, const Vector3 &ori = VEC3_ZERO);
        ~TerrainQuadTree() = default;

        using NodeIndex = uint32_t;
        static constexpr NodeIndex INVALID_IDX = ~(0U);

        struct TreeNode {
            TerrainCoord coord  = {};
            uint32_t     extent = 0;
            uint32_t     level  = 0;

            NodeIndex idxChild    = INVALID_IDX;
            uint32_t  numElements = 0;
        };

        void Split(const Vector3 &focus);
    private:
        void SplitChild(NodeIndex index, const TerrainCoord &focusCoord);
        NodeIndex AllocateNode();

        TerrainConfig config;
        Vector3 origin;

        std::vector<TreeNode>  nodes;
        std::vector<NodeIndex> parentLinks;
    };

} // namespace sky
