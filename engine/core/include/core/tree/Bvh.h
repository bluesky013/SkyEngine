//
// Created by Zach Lee on 2026/2/24.
//

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <core/math/MathUtil.h>
#include <core/math/Vector3.h>
#include <core/shapes/AABB.h>
#include <core/shapes/Shapes.h>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <vector>

namespace sky {

    static constexpr uint32_t BVH_NULL = ~(0U);

    struct BvhNode {
        AABB     bounds;
        uint32_t left  = BVH_NULL; // left child or first element index (for leaf)
        uint32_t right = BVH_NULL; // right child (BVH_NULL for leaf)
        uint32_t count = 0;        // element count, 0 for internal nodes

        bool IsLeaf() const
        {
            return count > 0;
        }
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
        struct RayCursor {
            std::array<uint32_t, 64> stack{};
            uint32_t                 size = 0, element = 0, end = 0;
            bool                     started = false;
        };
        // A cursor belongs to this immutable tree; rebuilding invalidates it.
        template <typename Func>
        bool QueryRayStep(const Vector3 &origin, const Vector3 &dir, float tMax, RayCursor &cursor, uint32_t &work, const Func &func) const;
        struct BuildStatistics {
            double copyMilliseconds = 0, boundsMilliseconds = 0, partitionMilliseconds = 0;
            size_t scratchBytes = 0;
        };
        const BuildStatistics &GetBuildStatistics() const
        {
            return buildStatistics;
        }
        static constexpr size_t BuildScratchBytes(size_t count)
        {
            return count * (sizeof(AABB) + sizeof(Vector3));
        }
        static constexpr uint32_t MAX_LEAF_SIZE = 4;

        Bvh()  = default;
        ~Bvh() = default;

        /**
         * @brief Build the BVH from a collection of elements.
         */
        void Build(const std::vector<T> &elements, const std::atomic<bool> *cancelled = nullptr);

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
        bool Empty() const
        {
            return elements_.empty();
        }

        /**
         * @brief Get the number of elements in the BVH.
         */
        uint32_t GetElementCount() const
        {
            return static_cast<uint32_t>(elements_.size());
        }

        /**
         * @brief Get the number of nodes in the BVH.
         */
        uint32_t GetNodeCount() const
        {
            return static_cast<uint32_t>(nodes_.size());
        }

        size_t GetMemoryUsage() const
        {
            return nodes_.capacity() * sizeof(BvhNode) + elements_.capacity() * sizeof(T);
        }

        /**
         * @brief Get the root bounding box.
         */
        const AABB &GetRootBounds() const
        {
            return nodes_.empty() ? emptyBounds_ : nodes_[0].bounds;
        }

    private:
        uint32_t AllocNode();

        void BuildRecursive(uint32_t nodeIdx, uint32_t begin, uint32_t end, uint32_t depth, const std::atomic<bool> *cancelled);

        AABB ComputeBounds(uint32_t begin, uint32_t end) const;

        static float SurfaceArea(const AABB &box);

        static bool RayAABBIntersect(const Vector3 &origin, const Vector3 &dir, const AABB &box, float tMax);

        BuildStatistics buildStatistics;
        struct BuildBounds {
            AABB    bounds;
            Vector3 centroid;
        };
        std::vector<BuildBounds> buildBounds;
        std::vector<BvhNode>     nodes_;
        std::vector<T>           elements_;

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
    void Bvh<T>::Build(const std::vector<T> &elements, const std::atomic<bool> *cancelled)
    {
        Clear();
        buildStatistics    = {};
        using Clock        = std::chrono::steady_clock;
        auto       start   = Clock::now();
        const auto elapsed = [&] { return std::chrono::duration<double, std::milli>(Clock::now() - start).count(); };
        if (elements.empty() || (cancelled && cancelled->load(std::memory_order_relaxed))) {
            return;
        }

        elements_ = elements;
        nodes_.reserve(elements_.size() * 2);
        buildStatistics.copyMilliseconds = elapsed();
        start                            = Clock::now();

        buildBounds.resize(elements_.size());
        buildStatistics.scratchBytes = buildBounds.capacity() * sizeof(BuildBounds);
        for (size_t i = 0; i < elements_.size(); ++i) {
            if (cancelled && cancelled->load(std::memory_order_relaxed)) {
                Clear();
                std::vector<BuildBounds>().swap(buildBounds);
                return;
            }
            const auto bounds = BvhTraits<T>::GetBounds(elements_[i]);
            buildBounds[i]    = {bounds, bounds.min * .5f + bounds.max * .5f};
        }
        uint32_t root                      = AllocNode();
        buildStatistics.boundsMilliseconds = elapsed();
        start                              = Clock::now();
        BuildRecursive(root, 0, static_cast<uint32_t>(elements_.size()), 0, cancelled);
        buildStatistics.partitionMilliseconds = elapsed();
        std::vector<BuildBounds>().swap(buildBounds);
        if (cancelled && cancelled->load(std::memory_order_relaxed))
            Clear();
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
        result.min = Vector3(std::numeric_limits<float>::max());
        result.max = Vector3(-std::numeric_limits<float>::max());

        for (uint32_t i = begin; i < end; ++i) {
            const AABB &b = BvhTraits<T>::GetBounds(elements_[i]);
            result.min    = Min(result.min, b.min);
            result.max    = Max(result.max, b.max);
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
    void Bvh<T>::BuildRecursive(uint32_t nodeIdx, uint32_t begin, uint32_t end, uint32_t depth, const std::atomic<bool> *cancelled)
    {
        if (cancelled && cancelled->load(std::memory_order_relaxed))
            return;
        const uint32_t count = end - begin;
        const AABB     empty{Vector3{std::numeric_limits<float>::max()}, Vector3{-std::numeric_limits<float>::max()}};
        AABB           bounds = empty, centers = empty;
        const auto     merge = [](AABB &a, const AABB &b) {
            a.min = Min(a.min, b.min);
            a.max = Max(a.max, b.max);
        };
        for (uint32_t i = begin; i < end; ++i) {
            if ((i & 1023u) == 0 && cancelled && cancelled->load(std::memory_order_relaxed))
                return;
            merge(bounds, buildBounds[i].bounds);
            centers.min = Min(centers.min, buildBounds[i].centroid);
            centers.max = Max(centers.max, buildBounds[i].centroid);
        }
        nodes_[nodeIdx].bounds = bounds;
        if (count <= MAX_LEAF_SIZE || depth >= 48) {
            nodes_[nodeIdx].left  = begin;
            nodes_[nodeIdx].count = count;
            return;
        }
        constexpr uint32_t BIN_COUNT = 16;
        struct Bin {
            AABB     bounds;
            uint32_t count = 0;
        };
        float      bestCost = std::numeric_limits<float>::max();
        int        bestAxis = -1;
        uint32_t   bestBin  = 0;
        const auto binFor   = [&](uint32_t i, int axis) {
            const double span     = double(centers.max[axis]) - centers.min[axis];
            const double relative = (double(buildBounds[i].centroid[axis]) - centers.min[axis]) / span;
            return static_cast<uint32_t>(std::clamp(relative * BIN_COUNT, 0.0, double(BIN_COUNT - 1)));
        };
        for (int axis = 0; axis < 3; ++axis) {
            if (!(centers.max[axis] > centers.min[axis]))
                continue;
            std::array<Bin, BIN_COUNT> bins;
            for (auto &bin : bins)
                bin.bounds = empty;
            for (uint32_t i = begin; i < end; ++i) {
                if ((i & 1023u) == 0 && cancelled && cancelled->load(std::memory_order_relaxed))
                    return;
                auto &bin = bins[binFor(i, axis)];
                merge(bin.bounds, buildBounds[i].bounds);
                ++bin.count;
            }
            std::array<float, BIN_COUNT> rightCosts{};
            AABB                         right      = empty;
            uint32_t                     rightCount = 0;
            for (uint32_t i = BIN_COUNT; i-- > 0;) {
                if (bins[i].count)
                    merge(right, bins[i].bounds);
                rightCount += bins[i].count;
                rightCosts[i] = rightCount ? SurfaceArea(right) * rightCount : 0;
            }
            AABB     left      = empty;
            uint32_t leftCount = 0;
            for (uint32_t i = 0; i + 1 < BIN_COUNT; ++i) {
                if (bins[i].count)
                    merge(left, bins[i].bounds);
                leftCount += bins[i].count;
                if (!leftCount || leftCount == count)
                    continue;
                const float cost = SurfaceArea(left) * leftCount + rightCosts[i + 1];
                if (cost < bestCost) {
                    bestCost = cost;
                    bestAxis = axis;
                    bestBin  = i;
                }
            }
        }
        if (bestAxis >= 0 && bestCost >= SurfaceArea(bounds) * count) {
            nodes_[nodeIdx].left  = begin;
            nodes_[nodeIdx].count = count;
            return;
        }
        uint32_t split = begin + count / 2;
        if (bestAxis >= 0) {
            uint32_t lo = begin, hi = end;
            while (lo < hi) {
                if ((lo & 1023u) == 0 && cancelled && cancelled->load(std::memory_order_relaxed))
                    return;
                if (binFor(lo, bestAxis) <= bestBin) {
                    ++lo;
                } else {
                    --hi;
                    std::swap(elements_[lo], elements_[hi]);
                    std::swap(buildBounds[lo], buildBounds[hi]);
                }
            }
            if (lo > begin && lo < end)
                split = lo;
        }
        const uint32_t left = AllocNode(), right = AllocNode();
        nodes_[nodeIdx].left  = left;
        nodes_[nodeIdx].right = right;
        BuildRecursive(left, begin, split, depth + 1, cancelled);
        BuildRecursive(right, split, end, depth + 1, cancelled);
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
    bool Bvh<T>::RayAABBIntersect(const Vector3 &origin, const Vector3 &dir, const AABB &box, float tMax)
    {
        double near = 0, far = tMax;
        for (int axis = 0; axis < 3; ++axis) {
            if (dir[axis] == 0) {
                if (origin[axis] < box.min[axis] || origin[axis] > box.max[axis])
                    return false;
                continue;
            }
            const double a = (double(box.min[axis]) - origin[axis]) / dir[axis];
            const double b = (double(box.max[axis]) - origin[axis]) / dir[axis];
            near           = std::max(near, std::min(a, b));
            far            = std::min(far, std::max(a, b));
            if (near > far)
                return false;
        }
        return true;
    }

    template <typename T>
    template <typename Func>
    bool Bvh<T>::QueryRayStep(const Vector3 &origin, const Vector3 &dir, float tMax, RayCursor &cursor, uint32_t &work, const Func &func) const
    {
        if (!cursor.started) {
            cursor.started = true;
            if (!nodes_.empty())
                cursor.stack[cursor.size++] = 0;
        }
        while (work && (cursor.size || cursor.element < cursor.end)) {
            --work;
            if (cursor.element < cursor.end) {
                const auto &element = elements_[cursor.element++];
                if (RayAABBIntersect(origin, dir, BvhTraits<T>::GetBounds(element), tMax))
                    func(element);
                continue;
            }
            const auto &node = nodes_[cursor.stack[--cursor.size]];
            if (!RayAABBIntersect(origin, dir, node.bounds, tMax)) {
                continue;
            }
            if (node.IsLeaf()) {
                cursor.element = node.left;
                cursor.end     = node.left + node.count;
            } else {
                cursor.stack[cursor.size++] = node.left;
                cursor.stack[cursor.size++] = node.right;
            }
        }
        return !cursor.size && cursor.element == cursor.end;
    }

    template <typename T>
    template <typename Func>
    void Bvh<T>::QueryRay(const Vector3 &origin, const Vector3 &dir, float tMax, const Func &func) const
    {
        RayCursor cursor;
        uint32_t  work = UINT32_MAX;
        while (!QueryRayStep(origin, dir, tMax, cursor, work, func))
            work = UINT32_MAX;
    }

} // namespace sky
