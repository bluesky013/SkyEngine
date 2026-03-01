//
// Created by blues on 2024/10/19.
//

#pragma once

#include <core/tree/OctTree.h>
#include <core/template/ReferenceObject.h>
#include <core/shapes/TriangleMesh.h>

namespace sky {
    struct NaviOctreeElement;
    using NaviOctreeElementRef = CounterPtr<NaviOctreeElement>;
    using NaviOctree = Octree<NaviOctreeElementRef>;

    template<>
    struct OctreeTraits<NaviOctreeElementRef> {
        using BoundType = AABB;
        static const uint32_t MAX_DEPTH = 12;
        static const uint32_t MAX_ELEMENT_LEAF = 16;

        static inline const BoundType &GetBounds(const NaviOctreeElementRef &ele);
        static inline void IndexChanged(const NaviOctreeElementRef &ele, const NaviOctree::ElementIndex &index);
    };

    struct NaviOctreeElement : public RefObject {
        CounterPtr<TriangleMesh> triangleMesh;
        uint32_t viewIndex = 0;

        const AABB &GetBounds() const
        {
            return triangleMesh->views[viewIndex].aabb;
        }

        NaviOctree::ElementIndex index;
    };

    inline const AABB &OctreeTraits<NaviOctreeElementRef>::GetBounds(const NaviOctreeElementRef &ele)
    {
        return ele->GetBounds();
    }

    inline void OctreeTraits<NaviOctreeElementRef>::IndexChanged(const NaviOctreeElementRef &ele, const NaviOctree::ElementIndex &index)
    {
        ele->index = index;
    }
}
