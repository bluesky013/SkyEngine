//
// Created by Zach Lee on 2026/2/24.
//

#pragma once

#include <vector>
#include <algorithm>
#include <numeric>
#include <functional>
#include <limits>
#include <cstdint>
#include <core/math/Vector3.h>
#include <core/math/MathUtil.h>
#include <core/shapes/AABB.h>
#include <core/shapes/Shapes.h>

namespace sky {

    static constexpr uint32_t BVH_NULL = ~(0U);

    struct BvhNode {
        AABB bounds;
        uint32_t left = BVH_NULL;   // left child or first element index (for leaf)
        uint32_t right = BVH_NULL;  // right child (BVH_NULL for leaf)
        uint32_t count = 0;         // element count, 0 for internal nodes

        bool IsLeaf() const { return count > 0; }
    };

    /**
     * @brief Traits type that users must specialize for their element type T.
     *
     * Required members:
     *   static AABB GetBounds(const T &element);
     */
    template <typename T>
    struct BvhTraits;

    /**
     * @brief A template-based Bounding Volume Hierarchy.
     *
     * @tparam T  Element type. Must have a BvhTraits<T> specialization
     *            providing `static AABB GetBounds(const T&)`.
     *
     * The tree is built top-down using the Surface Area Heuristic (SAH).
     * It supports AABB overlap queries and ray intersection queries.
     */
    template <typename T>
    class Bvh {
    public:
        static constexpr uint32_t MAX_LEAF_SIZE = 4;

        Bvh() = default;
        ~Bvh() = default;

        /**
         * @brief Build the BVH from a collection of elements.
         */
        void Build(const std::vector<T> &elements);

        /**
         * @brief Query all elements whose bounds overlap the given AABB.
         */
        template <typename Func>
        void QueryAABB(const AABB &queryBounds, const Func &func) const;

        /**
         * @brief Query all elements whose bounds are intersected by a ray.
         * @param origin  Ray origin.
         * @param dir     Ray direction (does not need to be normalized).
         * @param tMax    Maximum ray parameter.
         */
        template <typename Func>
        void QueryRay(const Vector3 &origin, const Vector3 &dir, float tMax, const Func &func) const;

        /**
         * @brief Clear the BVH.
         */
        void Clear();

        /**
         * @brief Check if the BVH is empty (no elements).
         */
        bool Empty() const { return elements_.empty(); }

        /**
         * @brief Get the number of elements in the BVH.
         */
        uint32_t GetElementCount() const { return static_cast<uint32_t>(elements_.size()); }

        /**
         * @brief Get the number of nodes in the BVH.
         */
        uint32_t GetNodeCount() const { return static_cast<uint32_t>(nodes_.size()); }

        /**
         * @brief Get the root bounding box.
         */
        const AABB &GetRootBounds() const { return nodes_.empty() ? emptyBounds_ : nodes_[0].bounds; }

    private:
        uint32_t AllocNode();

        void BuildRecursive(uint32_t nodeIdx, uint32_t begin, uint32_t end);

        AABB ComputeBounds(uint32_t begin, uint32_t end) const;

        static float SurfaceArea(const AABB &box);

        static bool RayAABBIntersect(const Vector3 &origin, const Vector3 &invDir, const AABB &box, float tMax);

        std::vector<BvhNode> nodes_;
        std::vector<T> elements_;

        static inline const AABB emptyBounds_ = AABB(VEC3_ZERO, VEC3_ZERO);
    };

    // =========================================================================
    // Implementation
    // =========================================================================

    template <typename T>
    uint32_t Bvh<T>::AllocNode()
    {
        auto idx = static_cast<uint32_t>(nodes_.size());
        nodes_.emplace_back();
        return idx;
    }

    template <typename T>
    void Bvh<T>::Build(const std::vector<T> &elements)
    {
        Clear();
        if (elements.empty()) {
            return;
        }

        elements_ = elements;
        nodes_.reserve(elements_.size() * 2);

        uint32_t root = AllocNode();
        BuildRecursive(root, 0, static_cast<uint32_t>(elements_.size()));
    }

    template <typename T>
    void Bvh<T>::Clear()
    {
        nodes_.clear();
        elements_.clear();
    }

    template <typename T>
    AABB Bvh<T>::ComputeBounds(uint32_t begin, uint32_t end) const
    {
        AABB result;
        result.min = Vector3( std::numeric_limits<float>::max());
        result.max = Vector3(-std::numeric_limits<float>::max());

        for (uint32_t i = begin; i < end; ++i) {
            const AABB &b = BvhTraits<T>::GetBounds(elements_[i]);
            result.min = Min(result.min, b.min);
            result.max = Max(result.max, b.max);
        }
        return result;
    }

    template <typename T>
    float Bvh<T>::SurfaceArea(const AABB &box)
    {
        Vector3 d = box.max - box.min;
        return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
    }

    template <typename T>
    void Bvh<T>::BuildRecursive(uint32_t nodeIdx, uint32_t begin, uint32_t end)
    {
        auto &node = nodes_[nodeIdx];
        node.bounds = ComputeBounds(begin, end);

        uint32_t count = end - begin;
        if (count <= MAX_LEAF_SIZE) {
            node.left = begin;
            node.right = BVH_NULL;
            node.count = count;
            return;
        }

        // Find the best split using SAH over the 3 axes
        float bestCost = std::numeric_limits<float>::max();
        int bestAxis = -1;
        uint32_t bestSplit = begin;

        float parentArea = SurfaceArea(node.bounds);
        if (parentArea < 1e-12f) {
            // Degenerate bounds — make a leaf
            node.left = begin;
            node.right = BVH_NULL;
            node.count = count;
            return;
        }

        for (int axis = 0; axis < 3; ++axis) {
            // Sort elements along this axis by their centroid
            std::sort(elements_.begin() + begin, elements_.begin() + end,
                [axis](const T &a, const T &b) {
                    const AABB &ba = BvhTraits<T>::GetBounds(a);
                    const AABB &bb = BvhTraits<T>::GetBounds(b);
                    float ca = (ba.min[axis] + ba.max[axis]) * 0.5f;
                    float cb = (bb.min[axis] + bb.max[axis]) * 0.5f;
                    return ca < cb;
                });

            // Sweep to find the best split position
            // Precompute right-side surface areas
            std::vector<float> rightAreas(count);
            {
                AABB rightBox;
                rightBox.min = Vector3( std::numeric_limits<float>::max());
                rightBox.max = Vector3(-std::numeric_limits<float>::max());
                for (uint32_t i = count; i > 0; --i) {
                    const AABB &b = BvhTraits<T>::GetBounds(elements_[begin + i - 1]);
                    rightBox.min = Min(rightBox.min, b.min);
                    rightBox.max = Max(rightBox.max, b.max);
                    rightAreas[i - 1] = SurfaceArea(rightBox);
                }
            }

            // Sweep from left to evaluate SAH
            {
                AABB leftBox;
                leftBox.min = Vector3( std::numeric_limits<float>::max());
                leftBox.max = Vector3(-std::numeric_limits<float>::max());
                for (uint32_t i = 0; i < count - 1; ++i) {
                    const AABB &b = BvhTraits<T>::GetBounds(elements_[begin + i]);
                    leftBox.min = Min(leftBox.min, b.min);
                    leftBox.max = Max(leftBox.max, b.max);

                    float leftArea = SurfaceArea(leftBox);
                    float rArea = rightAreas[i + 1];

                    uint32_t leftCount = i + 1;
                    uint32_t rightCount = count - leftCount;

                    float cost = (leftArea * static_cast<float>(leftCount) +
                                  rArea * static_cast<float>(rightCount)) / parentArea;

                    if (cost < bestCost) {
                        bestCost = cost;
                        bestAxis = axis;
                        bestSplit = begin + leftCount;
                    }
                }
            }
        }

        // If no good split was found, create a leaf
        if (bestAxis < 0 || bestSplit == begin || bestSplit == end) {
            node.left = begin;
            node.right = BVH_NULL;
            node.count = count;
            return;
        }

        // Re-sort along the best axis if it wasn't the last axis sorted
        if (bestAxis != 2) {
            std::sort(elements_.begin() + begin, elements_.begin() + end,
                [bestAxis](const T &a, const T &b) {
                    const AABB &ba = BvhTraits<T>::GetBounds(a);
                    const AABB &bb = BvhTraits<T>::GetBounds(b);
                    float ca = (ba.min[bestAxis] + ba.max[bestAxis]) * 0.5f;
                    float cb = (bb.min[bestAxis] + bb.max[bestAxis]) * 0.5f;
                    return ca < cb;
                });
        }

        // Create child nodes
        uint32_t leftChild = AllocNode();
        uint32_t rightChild = AllocNode();

        // Re-fetch node reference as AllocNode may have invalidated it
        nodes_[nodeIdx].left = leftChild;
        nodes_[nodeIdx].right = rightChild;
        nodes_[nodeIdx].count = 0;

        BuildRecursive(leftChild, begin, bestSplit);
        BuildRecursive(rightChild, bestSplit, end);
    }

    template <typename T>
    template <typename Func>
    void Bvh<T>::QueryAABB(const AABB &queryBounds, const Func &func) const
    {
        if (nodes_.empty()) {
            return;
        }

        // Iterative traversal using a stack
        std::vector<uint32_t> stack;
        stack.reserve(64);
        stack.push_back(0);

        while (!stack.empty()) {
            uint32_t idx = stack.back();
            stack.pop_back();

            const auto &node = nodes_[idx];
            if (!Intersection(node.bounds, queryBounds)) {
                continue;
            }

            if (node.IsLeaf()) {
                for (uint32_t i = node.left; i < node.left + node.count; ++i) {
                    if (Intersection(BvhTraits<T>::GetBounds(elements_[i]), queryBounds)) {
                        func(elements_[i]);
                    }
                }
            } else {
                stack.push_back(node.left);
                stack.push_back(node.right);
            }
        }
    }

    template <typename T>
    bool Bvh<T>::RayAABBIntersect(const Vector3 &origin, const Vector3 &invDir, const AABB &box, float tMax)
    {
        float t1 = (box.min.x - origin.x) * invDir.x;
        float t2 = (box.max.x - origin.x) * invDir.x;
        float tNear = std::fmin(t1, t2);
        float tFar  = std::fmax(t1, t2);

        t1 = (box.min.y - origin.y) * invDir.y;
        t2 = (box.max.y - origin.y) * invDir.y;
        tNear = std::fmax(tNear, std::fmin(t1, t2));
        tFar  = std::fmin(tFar,  std::fmax(t1, t2));

        t1 = (box.min.z - origin.z) * invDir.z;
        t2 = (box.max.z - origin.z) * invDir.z;
        tNear = std::fmax(tNear, std::fmin(t1, t2));
        tFar  = std::fmin(tFar,  std::fmax(t1, t2));

        return tNear <= tFar && tFar >= 0.0f && tNear <= tMax;
    }

    template <typename T>
    template <typename Func>
    void Bvh<T>::QueryRay(const Vector3 &origin, const Vector3 &dir, float tMax, const Func &func) const
    {
        if (nodes_.empty()) {
            return;
        }

        // Compute inverse direction, handling near-zero components
        constexpr float EPS = 1e-8f;
        Vector3 invDir(
            std::fabs(dir.x) > EPS ? 1.0f / dir.x : (dir.x >= 0.0f ?  1.0f / EPS : -1.0f / EPS),
            std::fabs(dir.y) > EPS ? 1.0f / dir.y : (dir.y >= 0.0f ?  1.0f / EPS : -1.0f / EPS),
            std::fabs(dir.z) > EPS ? 1.0f / dir.z : (dir.z >= 0.0f ?  1.0f / EPS : -1.0f / EPS)
        );

        std::vector<uint32_t> stack;
        stack.reserve(64);
        stack.push_back(0);

        while (!stack.empty()) {
            uint32_t idx = stack.back();
            stack.pop_back();

            const auto &node = nodes_[idx];
            if (!RayAABBIntersect(origin, invDir, node.bounds, tMax)) {
                continue;
            }

            if (node.IsLeaf()) {
                for (uint32_t i = node.left; i < node.left + node.count; ++i) {
                    if (RayAABBIntersect(origin, invDir, BvhTraits<T>::GetBounds(elements_[i]), tMax)) {
                        func(elements_[i]);
                    }
                }
            } else {
                stack.push_back(node.left);
                stack.push_back(node.right);
            }
        }
    }

} // namespace sky