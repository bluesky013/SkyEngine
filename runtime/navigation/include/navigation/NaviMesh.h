//
// Created by blues on 2024/9/1.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/shapes/AABB.h>
#include <navigation/NavigationOctree.h>
#include <memory>

namespace sky::ai {
    struct NaviAgentConfig {
        float height = 2.f;
        float radius = 0.5f;
        float maxSlope = 45.f;
        float maxClimb = 0.2f;
    };

    struct NaviMeshResolution {
        float cellSize   = 0.25f;  // voxel xy
        float cellHeight = 0.1f;   // voxel z
        float tileSize   = 10.f;
    };

    struct INaviTriangleMesh {
        virtual const float* GetVertices() const = 0;
        virtual int* GetTriangles() const = 0;
        virtual int GetTriangleNum() const = 0;
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

    protected:
        NaviAgentConfig    agentCfg;
        NaviMeshResolution resolution;

        AABB buildBounds;
        std::unique_ptr<NaviOctree> octree;
    };

} // namespace sky::ai
