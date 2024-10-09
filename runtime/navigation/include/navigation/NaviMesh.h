//
// Created by blues on 2024/9/1.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/template/ReferenceObject.h>
#include <core/shapes/AABB.h>

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

    class NaviMeshData {
    public:
        NaviMeshData() = default;
        virtual ~NaviMeshData() = default;
    };

    class NaviMesh : public RefObject {
    public:
        NaviMesh() = default;
        ~NaviMesh() override = default;

        const NaviAgentConfig &GetAgentConfig() const { return agentCfg; }
        const NaviMeshResolution &GetResolution() const { return resolution; }

        void SetBounds(const AABB &volume) { bounds = volume; }
        const AABB& GetBounds() const { return bounds; }
    private:
        NaviAgentConfig    agentCfg;
        NaviMeshResolution resolution;

        AABB bounds;
    };

    class NaviMeshFactory : public Singleton<NaviMeshFactory> {
    public:
        NaviMeshFactory() = default;
        ~NaviMeshFactory() override = default;

        NaviMesh* CreateNaviMap();

        class Impl {
        public:
            Impl() = default;
            virtual ~Impl() = default;

            virtual NaviMesh* CreateNaviMesh() = 0;
        };

    private:
        std::unique_ptr<Impl> factory;
    };

} // namespace sky::ai
