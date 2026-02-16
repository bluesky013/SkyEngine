//
// Created for volume management system
//

#pragma once

#include <vector>
#include <cstdint>
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

        mutable Octree<VolumeEntry>::ElementIndex octreeIndex;
    };

    template <>
    struct OctreeTraits<VolumeEntry> {
        static constexpr uint32_t MAX_DEPTH = 8;
        static constexpr uint32_t MAX_ELEMENT_LEAF = 8;

        using BoundType = AABB;

        static const AABB &GetBounds(const VolumeEntry &entry)
        {
            return entry.worldBound;
        }

        static void IndexChanged(const VolumeEntry &entry, const Octree<VolumeEntry>::ElementIndex &index)
        {
            entry.octreeIndex = index;
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
            auto &entry = entries.emplace_back(VolumeEntry{id, worldBound, userData});
            octree.AddElement(entry);
            return id;
        }

        void RemoveVolume(VolumeID id)
        {
            for (size_t i = 0; i < entries.size(); ++i) {
                if (entries[i].id == id) {
                    octree.RemoveElement(entries[i].octreeIndex);
                    entries[i] = entries.back();
                    entries.pop_back();
                    return;
                }
            }
        }

        void UpdateVolume(VolumeID id, const AABB &newBound)
        {
            for (auto &entry : entries) {
                if (entry.id == id) {
                    octree.RemoveElement(entry.octreeIndex);
                    entry.worldBound = newBound;
                    octree.AddElement(entry);
                    return;
                }
            }
        }

        template <typename Func>
        void FrustumCull(const Frustum &frustum, const Func &callback) const
        {
            for (const auto &entry : entries) {
                if (Intersection(entry.worldBound, frustum)) {
                    callback(entry);
                }
            }
        }

        template <typename Func>
        void QueryByAABB(const AABB &queryBound, const Func &callback)
        {
            octree.ForeachWithBoundTest(queryBound, [&callback](const VolumeEntry &entry) {
                callback(entry);
            });
        }

        size_t GetVolumeCount() const { return entries.size(); }

        const VolumeEntry *FindVolume(VolumeID id) const
        {
            for (const auto &entry : entries) {
                if (entry.id == id) {
                    return &entry;
                }
            }
            return nullptr;
        }

    private:
        VolumeID nextID = 0;
        std::vector<VolumeEntry> entries;
        Octree<VolumeEntry> octree;
    };

} // namespace sky
