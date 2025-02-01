//
// Created by blues on 2024/9/1.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/shapes/AABB.h>
#include <navigation/NavigationOctree.h>
#include <navigation/NaviQueryFilter.h>
#include <memory>

namespace sky {
    class World;
} // namespace sky

namespace sky::ai {
    class NavigationSystem;

    struct NaviAgentConfig {
        float height = 2.f;
        float radius = 1.f;
        float maxSlope = 45.f;
        float maxClimb = 0.2f;
    };

    struct NaviMeshResolution {
        float cellSize   = 0.25f;  // voxel xy
        float cellHeight = 0.3f;   // voxel z
        float tileSize   = 10.f;
    };

    struct NaviPathQueryParam {

    };

    enum class NaviQueryResult {
        SUCCESS = 0,
        FAILED
    };

    class NaviMesh : public RefObject {
    public:
        NaviMesh() = default;
        ~NaviMesh() override = default;

        const NaviAgentConfig &GetAgentConfig() const { return agentCfg; }
        const NaviMeshResolution &GetResolution() const { return resolution; }

        void SetBounds(const AABB &bounds) { buildBounds = bounds; }
        const AABB &GetBounds() const { return buildBounds; }

        void PrepareForBuild();
        NaviOctree *GetOctree() const { return octree.get(); }

        virtual NaviQueryResult FindPath(const Vector3 &start, const Vector3 &end, const NaviQueryFilterPtr& filter, const NaviPathQueryParam &param) const = 0;

    protected:
        friend class NavigationSystem;
        virtual void OnAttachToWorld(World &world) {}
        virtual void OnDetachFromWorld(World &world) {}

        NavigationSystem* navSystem = nullptr;

        NaviAgentConfig    agentCfg;
        NaviMeshResolution resolution;

        AABB buildBounds;
        std::unique_ptr<NaviOctree> octree;
    };

} // namespace sky::ai
