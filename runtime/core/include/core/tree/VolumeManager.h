//
// Created for volume management system
//

#pragma once

#include <cstdint>
#include <unordered_map>
#include <core/shapes/AABB.h>
#include <core/shapes/Frustum.h>
#include <core/shapes/Shapes.h>
#include <core/tree/OctTree.h>

namespace sky {

    using VolumeID = uint32_t;
    static constexpr VolumeID INVALID_VOLUME_ID = ~(0U);

    struct VolumeEntry {
        VolumeID id = INVALID_VOLUME_ID;
        AABB worldBound;
        void *userData = nullptr;

        mutable Octree<VolumeEntry*>::ElementIndex octreeIndex;
    };

    template <>
    struct OctreeTraits<VolumeEntry*> {
        static constexpr uint32_t MAX_DEPTH = 8;
        static constexpr uint32_t MAX_ELEMENT_LEAF = 8;

        using BoundType = AABB;

        static const AABB &GetBounds(VolumeEntry *const &entry)
        {
            return entry->worldBound;
        }

        static void IndexChanged(VolumeEntry *const &entry, const Octree<VolumeEntry*>::ElementIndex &index)
        {
            entry->octreeIndex = index;
        }
    };

    class VolumeManager {
    public:
        explicit VolumeManager(float worldExtent = 2048.f, const Vector3 &origin = VEC3_ZERO)
            : octree(worldExtent, origin)
        {
        }

        ~VolumeManager() = default;

        VolumeID AddVolume(const AABB &worldBound, void *userData = nullptr)
        {
            VolumeID id = nextID++;
            auto [it, ok] = entries.emplace(id, VolumeEntry{id, worldBound, userData});
            octree.AddElement(&it->second);
            return id;
        }

        void RemoveVolume(VolumeID id)
        {
            auto it = entries.find(id);
            if (it != entries.end()) {
                octree.RemoveElement(it->second.octreeIndex);
                entries.erase(it);
            }
        }

        void UpdateVolume(VolumeID id, const AABB &newBound)
        {
            auto it = entries.find(id);
            if (it != entries.end()) {
                octree.RemoveElement(it->second.octreeIndex);
                it->second.worldBound = newBound;
                octree.AddElement(&it->second);
            }
        }

        template <typename Func>
        void FrustumCull(const Frustum &frustum, const Func &callback)
        {
            octree.ForeachWithNodeTest(
                [&frustum](const AABB &nodeBox) {
                    return Intersection(nodeBox, frustum);
                },
                [&callback, &frustum](VolumeEntry *entry) {
                    if (Intersection(entry->worldBound, frustum)) {
                        callback(*entry);
                    }
                }
            );
        }

        template <typename Func>
        void QueryByAABB(const AABB &queryBound, const Func &callback)
        {
            octree.ForeachWithBoundTest(queryBound, [&callback](VolumeEntry *entry) {
                callback(*entry);
            });
        }

        size_t GetVolumeCount() const { return entries.size(); }

        const VolumeEntry *FindVolume(VolumeID id) const
        {
            auto it = entries.find(id);
            return it != entries.end() ? &it->second : nullptr;
        }

    private:
        VolumeID nextID = 0;
        std::unordered_map<VolumeID, VolumeEntry> entries;
        Octree<VolumeEntry*> octree;
    };

} // namespace sky
