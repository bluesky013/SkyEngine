//
// Created by blues on 2024/10/18.
//

#pragma once

#include <vector>
#include <core/math/Vector3.h>
#include <core/math/MathUtil.h>
#include <core/shapes/Shapes.h>

namespace sky {

    struct OctreeChild {
        uint8_t index;

        static constexpr uint8_t INVALID = 8;
        explicit constexpr OctreeChild(uint8_t id) : index(id) {}
        constexpr OctreeChild() : index(INVALID) {}
        constexpr OctreeChild(uint8_t x, uint8_t y, uint8_t z)
        {
            index = (x << 0) | (y << 1) | (z << 2);
        }

        Vector3 Offset(const Vector3 &ori, float ext) const
        {
            auto ix = static_cast<float>((((index >> 0) & 1) << 1) - 1);
            auto iy = static_cast<float>((((index >> 1) & 1) << 1) - 1);
            auto iz = static_cast<float>((((index >> 2) & 1) << 1) - 1);

            return ori + Vector3(ix, iy, iz) * ext;
        }

        bool IsValid() const
        {
            return index != INVALID;
        }
    };

    template <typename T>
    struct OctreeTraits {
        static const uint32_t MAX_DEPTH = 8;
    };

    template <typename T>
    struct OctreeBoundTraits {};

    template <typename T>
    class Octree {
    public:
        using NodeIndex = uint32_t;
        static constexpr NodeIndex INVALID_IDX = ~(0U);

        using TreeTraits = OctreeTraits<T>;
        using BoundType = typename TreeTraits::BoundType;
        using BoundTraits = OctreeBoundTraits<BoundType>;

        static constexpr uint32_t MAX_DEPTH        = TreeTraits ::MAX_DEPTH;
        static constexpr uint32_t MAX_ELEMENT_LEAF = TreeTraits ::MAX_ELEMENT_LEAF;

        struct ElementIndex {
            NodeIndex nodeIndex = INVALID_IDX;
            uint32_t eleIndex   = INVALID_IDX;
        };

        struct TreeNode {
            Vector3 origin = VEC3_ZERO;
            float ext = 0.f;

            NodeIndex idxChild = INVALID_IDX;
            uint32_t numElements  = 0;
            bool IsLeaf() const { return idxChild == INVALID_IDX; }
        };

        void AddElement(const T &element)
        {
            const auto &bounds = TreeTraits::GetBounds(element);
            Insert(0, element, bounds);
        }

        void RemoveElement(ElementIndex id)
        {
            auto &elementList = elements[id.nodeIndex];

            if (id.eleIndex < elementList.size()) {
                elementList[id.eleIndex] = elementList.back();
                TreeTraits::IndexChanged(elementList[id.eleIndex], id);
            }
            elementList.pop_back();

            NodeIndex tmpIdx = id.nodeIndex;
            while (true) {
                nodes[tmpIdx].numElements--;
                if (tmpIdx == 0) {
                    break;
                }
                tmpIdx = parentLinks[(tmpIdx - 1) / 8];
            }
        }

        template <typename Func>
        void ForeachWithBoundTestWith(NodeIndex index, const BoundType &bounds, const Func &func)
        {
            if (nodes[index].numElements > 0) {

                for (auto &element : elements[index]) {
                    if (BoundTraits::Intersect(TreeTraits::GetBounds(element), bounds)) {
                        func(element);
                    }
                }

                if (!nodes[index].IsLeaf()) {
                    auto childStart = nodes[index].idxChild;
                    for (uint8_t i = 0; i < 8; ++i) {
                        ForeachWithBoundTestWith(childStart + i, bounds, func);
                    }
                }
            }
        }

        template <typename Func>
        void ForeachWithBoundTest(const BoundType &bounds, const Func &func)
        {
            ForeachWithBoundTestWith(0, bounds, func);
        }

        template <typename NodeTest, typename Func>
        void ForeachWithNodeTestWith(NodeIndex index, const NodeTest &nodeTest, const Func &func)
        {
            if (nodes[index].numElements > 0) {
                AABB nodeBox{
                    nodes[index].origin - Vector3(nodes[index].ext),
                    nodes[index].origin + Vector3(nodes[index].ext)
                };
                if (!nodeTest(nodeBox)) {
                    return;
                }

                for (auto &element : elements[index]) {
                    func(element);
                }

                if (!nodes[index].IsLeaf()) {
                    auto childStart = nodes[index].idxChild;
                    for (uint8_t i = 0; i < 8; ++i) {
                        ForeachWithNodeTestWith(childStart + i, nodeTest, func);
                    }
                }
            }
        }

        template <typename NodeTest, typename Func>
        void ForeachWithNodeTest(const NodeTest &nodeTest, const Func &func)
        {
            ForeachWithNodeTestWith(0, nodeTest, func);
        }

        explicit Octree(float maxExtent, const Vector3 &ori = VEC3_ZERO)
            : leafExtent(maxExtent * std::pow(0.5f, static_cast<float>(MAX_DEPTH)))
        {
            nodes.emplace_back(TreeNode{ori, maxExtent});
            elements.emplace_back();
        }

        ~Octree() = default;
    private:
        NodeIndex AllocateNodes()
        {
            NodeIndex index ;
            if (!freeList.empty()) {
                index = freeList.back();
                freeList.pop_back();
            } else {
                index = static_cast<NodeIndex>(nodes.size());
                nodes.resize(nodes.size() + 8);
                elements.resize(elements.size() + 8);
                parentLinks.emplace_back();
            }
            return index;
        }

        OctreeChild GetChild(const TreeNode &node, const BoundType &queryBounds)
        {
            auto queryCenter = BoundTraits::GetCenter(queryBounds);
            auto queryExtent = BoundTraits::GetExtent(queryBounds);

            const auto &currentCenter = node.origin;

            auto diffCenter = queryCenter - currentCenter;
            auto childExt   = node.ext / 4.f;

            auto negativeCenterDiff = queryCenter - (currentCenter - Vector3(childExt));
            auto positiveCenterDiff = (currentCenter + Vector3(childExt)) - queryCenter;
            auto minDiff = Min(negativeCenterDiff, positiveCenterDiff);
            auto queryDiff = queryExtent + minDiff;

            if (queryDiff.x > childExt ||
                queryDiff.y > childExt ||
                queryDiff.z > childExt) {
                return {};
            }

            uint8_t x = diffCenter.x > 0.f;
            uint8_t y = diffCenter.y > 0.f;
            uint8_t z = diffCenter.z > 0.f;
            return {x, y, z};
        }

        void Insert(NodeIndex current, const T& element, const BoundType &bounds)
        {
            nodes[current].numElements++;

            if (nodes[current].IsLeaf()) {
                // check leaf element num and extent
                if (elements[current].size() >= MAX_ELEMENT_LEAF && nodes[current].ext > leafExtent) {
                    std::vector<T> tmpElements = std::move(elements[current]);

                    auto childStart = AllocateNodes();
                    for (uint32_t i = 0; i < 8; ++i) {
                        nodes[childStart + i].origin = OctreeChild(i).Offset(nodes[current].origin, nodes[current].ext / 4.f);
                        nodes[childStart + i].ext = nodes[current].ext / 2.f;
                    }

                    parentLinks[(childStart - 1) / 8] = current;
                    nodes[current].idxChild    = childStart;
                    nodes[current].numElements = 0;

                    for (auto &tmp : tmpElements) {
                        const auto &tmpBounds = TreeTraits ::GetBounds(tmp);
                        Insert(current, tmp, tmpBounds);
                    }

                    Insert(current, element, bounds);
                } else {
                    ElementIndex index = {
                        current,
                        static_cast<uint32_t>(elements[current].size())
                    };
                    elements[current].emplace_back(element);
                    TreeTraits::IndexChanged(element, index);
                }
            } else {
                auto child = GetChild(nodes[current], bounds);
                if (child.IsValid()) {
                    auto nodeIndex = nodes[current].idxChild + child.index;
                    Insert(nodeIndex, element, bounds);
                } else {
                    ElementIndex index = {
                        current,
                        static_cast<uint32_t>(elements[current].size())
                    };
                    elements[current].emplace_back(element);
                    TreeTraits::IndexChanged(element, index);
                }
            }
        }

        float leafExtent;

        std::vector<TreeNode>       nodes;
        std::vector<NodeIndex>      parentLinks;
        std::vector<std::vector<T>> elements;
        std::vector<NodeIndex>      freeList;
    };

    template <>
    struct OctreeBoundTraits<AABB> {
        static Vector3 GetCenter(const AABB &bound)
        {
            return (bound.max + bound.min) / 2.f;
        }

        static Vector3 GetExtent(const AABB &bound)
        {
            return (bound.max - bound.min) / 2.f;
        }

        static bool Intersect(const AABB &lhs, const AABB &rhs)
        {
            return Intersection(lhs, rhs);
        }
    };

} // namespace sky
