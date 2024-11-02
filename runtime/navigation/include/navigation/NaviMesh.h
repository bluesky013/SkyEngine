//
// Created by blues on 2024/9/1.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/shapes/AABB.h>
#include <navigation/NavigationOctree.h>
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

        virtual void OnAttachToWorld(World &world) {}
        virtual void OnDetachFromWorld(World &world) {}
    protected:
        friend class NavigationSystem;
        NavigationSystem* navSystem = nullptr;

        NaviAgentConfig    agentCfg;
        NaviMeshResolution resolution;

        AABB buildBounds;
        std::unique_ptr<NaviOctree> octree;
    };

} // namespace sky::ai
