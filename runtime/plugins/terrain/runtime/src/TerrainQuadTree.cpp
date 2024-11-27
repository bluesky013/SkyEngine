//
// Created by blues on 2024/11/7.
//

#include <terrain/TerrainQuadTree.h>
#include <core/platform/Platform.h>
#include <algorithm>

namespace sky {

    TerrainQuadTree::TerrainQuadTree(const TerrainConfig &cfg, const Vector3 &ori)
        : config(cfg)
        , origin(ori)
    {
        auto maxLod = static_cast<uint32_t>(std::log2(cfg.maxExt / cfg.leafExt));
        nodes.emplace_back(TreeNode{{0, 0}, cfg.maxExt, maxLod});
    }

    void TerrainQuadTree::Split(const Vector3 &focusPos)
    {
        auto local = focusPos - origin;

        TerrainCoord focusCoord = {};
        focusCoord.x = static_cast<int32_t>(std::floor(local.x / config.resolution));
        focusCoord.y = static_cast<int32_t>(std::floor(local.y / config.resolution));

        SplitChild(0, focusCoord);
    }

    void TerrainQuadTree::SplitChild(NodeIndex index, const TerrainCoord &focusCoord) // NOLINT
    {
        // split child
        if (nodes[index].idxChild == INVALID_IDX) {
            auto childStart = AllocateNode();

            nodes[index].idxChild = childStart;
            parentLinks[(childStart - 1) / 4] = index;

            for (uint32_t i = 0; i < 4; ++i) {
                auto &child = nodes[childStart + i];

                child.coord  = QuadTreeChild(i).Offset(nodes[index].coord, nodes[index].extent / 4);
                child.extent = nodes[index].extent / 2;
                child.level  = nodes[index].level - 1;

                int diffX = child.coord.x - focusCoord.x;
                int diffY = child.coord.y - focusCoord.y;


                auto distToChild = static_cast<float>(sqrt(diffX * diffX + diffY * diffY)) * config.resolution;
                auto distHalfExt = static_cast<float>(child.extent) * config.resolution;

                if (distToChild < distHalfExt && child.extent > config.leafExt) {
                    SplitChild(childStart + i, focusCoord);
                }
            }
        }
    }

    TerrainQuadTree::NodeIndex TerrainQuadTree::AllocateNode()
    {
        auto index = static_cast<NodeIndex>(nodes.size());
        nodes.resize(nodes.size() + 4);
        parentLinks.emplace_back();

        return index;
    }

} // namespace sky