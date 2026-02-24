//
// Created by SkyEngine on 2024/02/24.
//

#pragma once

#include <vector>
#include <algorithm>
#include <functional>
#include <limits>
#include <cmath>
#include <core/math/Vector3.h>
#include <core/shapes/AABB.h>
#include <core/shapes/Shapes.h>

namespace sky {

    /**
     * @brief Traits for BVH element types
     * 
     * Specialize this template to define how to get bounds from your element type.
     * 
     * Example:
     * @code
     * template <>
     * struct BVHTraits<MyObject*> {
     *     static AABB GetBounds(MyObject* obj) {
     *         return obj->GetBounds();
     *     }
     * };
     * @endcode
     */
    template <typename T>
    struct BVHTraits {
        // Default implementation assumes T has a 'bounds' member
        static AABB GetBounds(const T &element)
        {
            return element.bounds;
        }
    };

    /**
     * @brief BVH build strategy
     */
    enum class BVHBuildStrategy {
        MEDIAN,         // Split at spatial median (fast build)
        SAH,            // Surface Area Heuristic (optimal traversal)
        OBJECT_MEDIAN   // Split at object median (balanced tree)
    };

    /**
     * @brief BVH configuration
     */
    struct BVHConfig {
        BVHBuildStrategy strategy = BVHBuildStrategy::OBJECT_MEDIAN;
        uint32_t maxElementsPerLeaf = 4;
        uint32_t maxDepth = 32;
        float sahTraversalCost = 1.0f;
        float sahIntersectionCost = 1.0f;
    };

    /**
     * @brief Bounding Volume Hierarchy for efficient spatial queries
     * 
     * A binary tree structure where each node contains an AABB that bounds
     * all elements in its subtree. Supports efficient ray casting, AABB
     * queries, and point queries.
     * 
     * @tparam T Element type stored in the BVH
     */
    template <typename T>
    class BVH {
    public:
        using NodeIndex = uint32_t;
        static constexpr NodeIndex INVALID_INDEX = ~(0U);

        /**
         * @brief BVH node structure
         */
        struct Node {
            AABB bounds;
            NodeIndex leftChild = INVALID_INDEX;   // Left child (if internal) or first element (if leaf)
            NodeIndex rightChild = INVALID_INDEX;  // Right child (if internal) or INVALID (if leaf)
            uint32_t elementStart = 0;             // Start index in elements array
            uint32_t elementCount = 0;             // Number of elements (0 for internal nodes)

            bool IsLeaf() const { return elementCount > 0; }
        };

        /**
         * @brief Result of a ray cast query
         */
        struct RayHit {
            bool hit = false;
            float distance = std::numeric_limits<float>::max();
            uint32_t elementIndex = INVALID_INDEX;
            const T *element = nullptr;
        };

        BVH() = default;
        ~BVH() = default;

        /**
         * @brief Build BVH from a list of elements
         * @param elements Elements to build BVH from
         * @param config Build configuration
         */
        void Build(const std::vector<T> &elements, const BVHConfig &config = BVHConfig{})
        {
            Clear();
            if (elements.empty()) {
                return;
            }

            config_ = config;
            
            // Store original elements temporarily
            std::vector<T> originalElements = elements;

            // Build index array for sorting without moving elements
            std::vector<uint32_t> indices(elements.size());
            for (uint32_t i = 0; i < elements.size(); ++i) {
                indices[i] = i;
            }

            // Compute centroids for SAH (using original indices)
            centroids_.resize(elements.size());
            for (uint32_t i = 0; i < elements.size(); ++i) {
                AABB bounds = BVHTraits<T>::GetBounds(originalElements[i]);
                centroids_[i] = (bounds.min + bounds.max) * 0.5f;
            }

            // Build the tree recursively - this sorts indices in place
            nodes_.reserve(elements.size() * 2);  // Rough estimate
            BuildRecursive(originalElements, indices, 0, static_cast<uint32_t>(indices.size()), 0);
            
            // Reorder elements to match the sorted indices
            // After this, elements_[i] corresponds to indices[i] from original
            elements_.resize(indices.size());
            for (uint32_t i = 0; i < indices.size(); ++i) {
                elements_[i] = originalElements[indices[i]];
            }
            
            // Clear centroids as they're no longer needed
            centroids_.clear();
        }

        /**
         * @brief Clear all data
         */
        void Clear()
        {
            nodes_.clear();
            elements_.clear();
            centroids_.clear();
        }

        /**
         * @brief Check if BVH is empty
         */
        bool IsEmpty() const { return nodes_.empty(); }

        /**
         * @brief Get the number of nodes
         */
        uint32_t GetNodeCount() const { return static_cast<uint32_t>(nodes_.size()); }

        /**
         * @brief Get the number of elements
         */
        uint32_t GetElementCount() const { return static_cast<uint32_t>(elements_.size()); }

        /**
         * @brief Get the root node
         */
        const Node& GetRoot() const { return nodes_[0]; }

        /**
         * @brief Query elements intersecting an AABB
         * @param queryBounds AABB to query
         * @param callback Function called for each intersecting element
         */
        template <typename Func>
        void QueryAABB(const AABB &queryBounds, Func &&callback) const
        {
            if (nodes_.empty()) {
                return;
            }
            QueryAABBRecursive(0, queryBounds, callback);
        }

        /**
         * @brief Collect elements intersecting an AABB
         * @param queryBounds AABB to query
         * @param result Vector to store intersecting elements
         */
        void QueryAABB(const AABB &queryBounds, std::vector<const T*> &result) const
        {
            result.clear();
            QueryAABB(queryBounds, [&result](const T &element) {
                result.push_back(&element);
            });
        }

        /**
         * @brief Cast a ray through the BVH
         * 
         * @param ray Ray to cast
         * @param maxDistance Maximum ray distance
         * @param hitTest Function to test intersection with element, returns (hit, distance)
         * @return RayHit result
         */
        template <typename HitTestFunc>
        RayHit RayCast(const Ray &ray, float maxDistance, HitTestFunc &&hitTest) const
        {
            RayHit result;
            result.distance = maxDistance;

            if (nodes_.empty()) {
                return result;
            }

            RayCastRecursive(0, ray, result, hitTest);
            return result;
        }

        /**
         * @brief Cast a ray with default AABB hit test
         * @param ray Ray to cast
         * @param maxDistance Maximum ray distance
         * @return RayHit result (distance is to AABB, not actual geometry)
         */
        RayHit RayCast(const Ray &ray, float maxDistance = std::numeric_limits<float>::max()) const
        {
            return RayCast(ray, maxDistance, [](const Ray &r, const T &element) {
                AABB bounds = BVHTraits<T>::GetBounds(element);
                auto [hit, tMin, tMax] = Intersection(r, bounds);
                return std::make_pair(hit, tMin);
            });
        }

        /**
         * @brief Query all elements that contain a point
         * @param point Point to test
         * @param callback Function called for each containing element
         */
        template <typename Func>
        void QueryPoint(const Vector3 &point, Func &&callback) const
        {
            if (nodes_.empty()) {
                return;
            }
            QueryPointRecursive(0, point, callback);
        }

        /**
         * @brief Iterate over all elements in the BVH
         * @param callback Function called for each element
         */
        template <typename Func>
        void ForEach(Func &&callback) const
        {
            for (const auto &element : elements_) {
                callback(element);
            }
        }

        /**
         * @brief Get the BVH depth
         */
        uint32_t GetDepth() const
        {
            if (nodes_.empty()) {
                return 0;
            }
            return GetDepthRecursive(0);
        }

        /**
         * @brief Get all nodes (for debugging/visualization)
         */
        const std::vector<Node>& GetNodes() const { return nodes_; }

        /**
         * @brief Get all elements
         */
        const std::vector<T>& GetElements() const { return elements_; }

    private:
        /**
         * @brief Build BVH recursively
         */
        NodeIndex BuildRecursive(const std::vector<T> &originalElements, std::vector<uint32_t> &indices, 
                                  uint32_t start, uint32_t end, uint32_t depth)
        {
            NodeIndex nodeIndex = static_cast<NodeIndex>(nodes_.size());
            nodes_.emplace_back();
            Node &node = nodes_.back();

            // Compute bounds for all elements in this node
            AABB bounds;
            bounds.min = Vector3(std::numeric_limits<float>::max());
            bounds.max = Vector3(std::numeric_limits<float>::lowest());

            for (uint32_t i = start; i < end; ++i) {
                AABB elementBounds = BVHTraits<T>::GetBounds(originalElements[indices[i]]);
                Merge(bounds, elementBounds, bounds);
            }
            node.bounds = bounds;

            uint32_t numElements = end - start;

            // Create leaf node if few elements or max depth reached
            if (numElements <= config_.maxElementsPerLeaf || depth >= config_.maxDepth) {
                node.elementStart = start;
                node.elementCount = numElements;
                return nodeIndex;
            }

            // Choose split axis and position
            uint32_t splitAxis;
            uint32_t splitIndex;
            FindBestSplit(originalElements, indices, start, end, bounds, splitAxis, splitIndex);

            // If split failed, create leaf
            if (splitIndex == start || splitIndex == end) {
                node.elementStart = start;
                node.elementCount = numElements;
                return nodeIndex;
            }

            // Partition elements
            PartitionElements(indices, start, end, splitAxis, splitIndex);

            // Recurse
            node.leftChild = BuildRecursive(originalElements, indices, start, splitIndex, depth + 1);
            // Re-fetch node reference as vector may have reallocated
            nodes_[nodeIndex].rightChild = BuildRecursive(originalElements, indices, splitIndex, end, depth + 1);

            return nodeIndex;
        }

        /**
         * @brief Find best split using configured strategy
         */
        void FindBestSplit(const std::vector<T> &originalElements, const std::vector<uint32_t> &indices, 
                           uint32_t start, uint32_t end,
                           const AABB &bounds, uint32_t &outAxis, uint32_t &outIndex)
        {
            switch (config_.strategy) {
                case BVHBuildStrategy::SAH:
                    FindBestSplitSAH(originalElements, indices, start, end, bounds, outAxis, outIndex);
                    break;
                case BVHBuildStrategy::MEDIAN:
                    FindBestSplitMedian(indices, start, end, bounds, outAxis, outIndex);
                    break;
                case BVHBuildStrategy::OBJECT_MEDIAN:
                default:
                    FindBestSplitObjectMedian(indices, start, end, bounds, outAxis, outIndex);
                    break;
            }
        }

        /**
         * @brief Find split using object median
         */
        void FindBestSplitObjectMedian(const std::vector<uint32_t> &indices, uint32_t start, uint32_t end,
                                       const AABB &bounds, uint32_t &outAxis, uint32_t &outIndex)
        {
            // Choose axis with largest extent
            Vector3 extent = bounds.max - bounds.min;
            if (extent.x >= extent.y && extent.x >= extent.z) {
                outAxis = 0;
            } else if (extent.y >= extent.z) {
                outAxis = 1;
            } else {
                outAxis = 2;
            }

            outIndex = (start + end) / 2;
        }

        /**
         * @brief Find split using spatial median
         */
        void FindBestSplitMedian(const std::vector<uint32_t> &indices, uint32_t start, uint32_t end,
                                 const AABB &bounds, uint32_t &outAxis, uint32_t &outIndex)
        {
            // Choose axis with largest extent
            Vector3 extent = bounds.max - bounds.min;
            if (extent.x >= extent.y && extent.x >= extent.z) {
                outAxis = 0;
            } else if (extent.y >= extent.z) {
                outAxis = 1;
            } else {
                outAxis = 2;
            }

            // Split at spatial median
            float splitPos = (GetAxisValue(bounds.min, outAxis) + GetAxisValue(bounds.max, outAxis)) * 0.5f;

            // Count elements on each side
            uint32_t leftCount = 0;
            for (uint32_t i = start; i < end; ++i) {
                if (GetAxisValue(centroids_[indices[i]], outAxis) < splitPos) {
                    ++leftCount;
                }
            }

            outIndex = start + leftCount;
            if (outIndex == start || outIndex == end) {
                outIndex = (start + end) / 2;  // Fall back to object median
            }
        }

        /**
         * @brief Find split using Surface Area Heuristic
         */
        void FindBestSplitSAH(const std::vector<T> &originalElements, const std::vector<uint32_t> &indices, 
                              uint32_t start, uint32_t end,
                              const AABB &bounds, uint32_t &outAxis, uint32_t &outIndex)
        {
            constexpr uint32_t NUM_BUCKETS = 12;
            float bestCost = std::numeric_limits<float>::max();
            outAxis = 0;
            outIndex = (start + end) / 2;

            uint32_t numElements = end - start;
            float invParentArea = 1.0f / SurfaceArea(bounds);

            // Try each axis
            for (uint32_t axis = 0; axis < 3; ++axis) {
                // Compute centroid bounds
                float minCentroid = std::numeric_limits<float>::max();
                float maxCentroid = std::numeric_limits<float>::lowest();
                for (uint32_t i = start; i < end; ++i) {
                    float c = GetAxisValue(centroids_[indices[i]], axis);
                    minCentroid = std::min(minCentroid, c);
                    maxCentroid = std::max(maxCentroid, c);
                }

                if (maxCentroid - minCentroid < 1e-6f) {
                    continue;  // All centroids at same position
                }

                // Initialize buckets
                struct Bucket {
                    uint32_t count = 0;
                    AABB bounds;
                };
                std::array<Bucket, NUM_BUCKETS> buckets;
                for (auto &b : buckets) {
                    b.bounds.min = Vector3(std::numeric_limits<float>::max());
                    b.bounds.max = Vector3(std::numeric_limits<float>::lowest());
                }

                // Place elements in buckets
                float scale = static_cast<float>(NUM_BUCKETS) / (maxCentroid - minCentroid);
                for (uint32_t i = start; i < end; ++i) {
                    float c = GetAxisValue(centroids_[indices[i]], axis);
                    uint32_t b = static_cast<uint32_t>((c - minCentroid) * scale);
                    b = std::min(b, NUM_BUCKETS - 1);
                    buckets[b].count++;
                    AABB elementBounds = BVHTraits<T>::GetBounds(originalElements[indices[i]]);
                    Merge(buckets[b].bounds, elementBounds, buckets[b].bounds);
                }

                // Compute costs for each split
                for (uint32_t split = 1; split < NUM_BUCKETS; ++split) {
                    AABB leftBounds, rightBounds;
                    leftBounds.min = rightBounds.min = Vector3(std::numeric_limits<float>::max());
                    leftBounds.max = rightBounds.max = Vector3(std::numeric_limits<float>::lowest());
                    uint32_t leftCount = 0, rightCount = 0;

                    for (uint32_t i = 0; i < split; ++i) {
                        if (buckets[i].count > 0) {
                            Merge(leftBounds, buckets[i].bounds, leftBounds);
                            leftCount += buckets[i].count;
                        }
                    }
                    for (uint32_t i = split; i < NUM_BUCKETS; ++i) {
                        if (buckets[i].count > 0) {
                            Merge(rightBounds, buckets[i].bounds, rightBounds);
                            rightCount += buckets[i].count;
                        }
                    }

                    if (leftCount == 0 || rightCount == 0) {
                        continue;
                    }

                    float cost = config_.sahTraversalCost +
                                 config_.sahIntersectionCost * (
                                     leftCount * SurfaceArea(leftBounds) +
                                     rightCount * SurfaceArea(rightBounds)) * invParentArea;

                    if (cost < bestCost) {
                        bestCost = cost;
                        outAxis = axis;
                        outIndex = start + leftCount;
                    }
                }
            }

            // Check if SAH split is better than leaf
            float leafCost = config_.sahIntersectionCost * static_cast<float>(numElements);
            if (bestCost >= leafCost) {
                outIndex = start;  // Signal to create leaf
            }
        }

        /**
         * @brief Partition elements around split
         */
        void PartitionElements(std::vector<uint32_t> &indices, uint32_t start, uint32_t end,
                               uint32_t axis, uint32_t splitIndex)
        {
            // Partial sort so that first (splitIndex - start) elements are on the left
            std::nth_element(indices.begin() + start, indices.begin() + splitIndex, indices.begin() + end,
                [this, axis](uint32_t a, uint32_t b) {
                    return GetAxisValue(centroids_[a], axis) < GetAxisValue(centroids_[b], axis);
                });
        }

        /**
         * @brief Get axis value from vector
         */
        static float GetAxisValue(const Vector3 &v, uint32_t axis)
        {
            switch (axis) {
                case 0: return v.x;
                case 1: return v.y;
                case 2: return v.z;
                default: return v.x;
            }
        }

        /**
         * @brief Compute surface area of AABB
         */
        static float SurfaceArea(const AABB &bounds)
        {
            Vector3 d = bounds.max - bounds.min;
            return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
        }

        /**
         * @brief Query AABB recursively
         */
        template <typename Func>
        void QueryAABBRecursive(NodeIndex nodeIndex, const AABB &queryBounds, Func &&callback) const
        {
            const Node &node = nodes_[nodeIndex];

            // Test node bounds
            if (!Intersection(node.bounds, queryBounds)) {
                return;
            }

            if (node.IsLeaf()) {
                // Test each element
                for (uint32_t i = 0; i < node.elementCount; ++i) {
                    const T &element = elements_[node.elementStart + i];
                    AABB elementBounds = BVHTraits<T>::GetBounds(element);
                    if (Intersection(elementBounds, queryBounds)) {
                        callback(element);
                    }
                }
            } else {
                // Recurse to children
                QueryAABBRecursive(node.leftChild, queryBounds, callback);
                QueryAABBRecursive(node.rightChild, queryBounds, callback);
            }
        }

        /**
         * @brief Ray cast recursively
         */
        template <typename HitTestFunc>
        void RayCastRecursive(NodeIndex nodeIndex, const Ray &ray, RayHit &result, HitTestFunc &&hitTest) const
        {
            const Node &node = nodes_[nodeIndex];

            // Test node bounds
            auto [hit, tMin, tMax] = Intersection(ray, node.bounds);
            if (!hit || tMin > result.distance) {
                return;
            }

            if (node.IsLeaf()) {
                // Test each element
                for (uint32_t i = 0; i < node.elementCount; ++i) {
                    uint32_t elementIndex = node.elementStart + i;
                    const T &element = elements_[elementIndex];
                    auto [elemHit, elemDist] = hitTest(ray, element);
                    if (elemHit && elemDist < result.distance && elemDist >= 0.0f) {
                        result.hit = true;
                        result.distance = elemDist;
                        result.elementIndex = elementIndex;
                        result.element = &element;
                    }
                }
            } else {
                // Order children by distance to ray origin for early termination
                const Node &leftNode = nodes_[node.leftChild];
                const Node &rightNode = nodes_[node.rightChild];

                auto [leftHit, leftTMin, leftTMax] = Intersection(ray, leftNode.bounds);
                auto [rightHit, rightTMin, rightTMax] = Intersection(ray, rightNode.bounds);

                if (leftHit && rightHit) {
                    // Visit closer child first
                    if (leftTMin < rightTMin) {
                        RayCastRecursive(node.leftChild, ray, result, hitTest);
                        if (rightTMin < result.distance) {
                            RayCastRecursive(node.rightChild, ray, result, hitTest);
                        }
                    } else {
                        RayCastRecursive(node.rightChild, ray, result, hitTest);
                        if (leftTMin < result.distance) {
                            RayCastRecursive(node.leftChild, ray, result, hitTest);
                        }
                    }
                } else if (leftHit) {
                    RayCastRecursive(node.leftChild, ray, result, hitTest);
                } else if (rightHit) {
                    RayCastRecursive(node.rightChild, ray, result, hitTest);
                }
            }
        }

        /**
         * @brief Query point recursively
         */
        template <typename Func>
        void QueryPointRecursive(NodeIndex nodeIndex, const Vector3 &point, Func &&callback) const
        {
            const Node &node = nodes_[nodeIndex];

            // Test if point is in node bounds
            if (point.x < node.bounds.min.x || point.x > node.bounds.max.x ||
                point.y < node.bounds.min.y || point.y > node.bounds.max.y ||
                point.z < node.bounds.min.z || point.z > node.bounds.max.z) {
                return;
            }

            if (node.IsLeaf()) {
                // Test each element
                for (uint32_t i = 0; i < node.elementCount; ++i) {
                    const T &element = elements_[node.elementStart + i];
                    AABB elementBounds = BVHTraits<T>::GetBounds(element);
                    if (point.x >= elementBounds.min.x && point.x <= elementBounds.max.x &&
                        point.y >= elementBounds.min.y && point.y <= elementBounds.max.y &&
                        point.z >= elementBounds.min.z && point.z <= elementBounds.max.z) {
                        callback(element);
                    }
                }
            } else {
                QueryPointRecursive(node.leftChild, point, callback);
                QueryPointRecursive(node.rightChild, point, callback);
            }
        }

        /**
         * @brief Get depth recursively
         */
        uint32_t GetDepthRecursive(NodeIndex nodeIndex) const
        {
            const Node &node = nodes_[nodeIndex];
            if (node.IsLeaf()) {
                return 1;
            }
            return 1 + std::max(GetDepthRecursive(node.leftChild), GetDepthRecursive(node.rightChild));
        }

        BVHConfig config_;
        std::vector<Node> nodes_;
        std::vector<T> elements_;
        std::vector<Vector3> centroids_;
    };

    /**
     * @brief Specialization for AABB-bounded objects
     */
    template <>
    struct BVHTraits<AABB> {
        static AABB GetBounds(const AABB &element)
        {
            return element;
        }
    };

} // namespace sky
