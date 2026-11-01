//
// Created by blues on 2024/9/1.
//

#pragma once

#include <core/shapes/AABB.h>
#include <core/template/ReferenceObject.h>
#include <memory>
#include <navigation/NaviDebugGeometry.h>
#include <navigation/NaviQueryFilter.h>
#include <navigation/NavigationOctree.h>

namespace sky {
    class World;
} // namespace sky

namespace sky::ai {
    class INavigationSystem;
    class NavigationSystem;
    struct NaviMeshData;
    struct NaviMeshBuildParams;
    struct NaviMeshTilePayload;
    struct NaviPath;

    struct NaviAgentConfig {
        float height   = 2.f;
        float radius   = 1.f;
        float maxSlope = 45.f;
        float maxClimb = 0.2f;
    };

    struct NaviMeshResolution {
        float cellSize   = 0.25f; // voxel xy
        float cellHeight = 0.3f;  // voxel z
        float tileSize   = 10.f;
    };

    struct NaviPathQueryParam {};

    struct NaviMeshTileCoord {
        int32_t x = 0;
        int32_t y = 0;
    };

    enum class NaviQueryResult { SUCCESS = 0, FAILED };

    class NaviMesh : public RefObject {
    public:
        NaviMesh()           = default;
        ~NaviMesh() override = default;

        const NaviAgentConfig &GetAgentConfig() const
        {
            return agentCfg;
        }
        const NaviMeshResolution &GetResolution() const
        {
            return resolution;
        }

        void SetAgentConfig(const NaviAgentConfig &cfg)
        {
            agentCfg = cfg;
        }
        void SetResolution(const NaviMeshResolution &res)
        {
            resolution = res;
        }

        void SetBounds(const AABB &bounds)
        {
            buildBounds = bounds;
        }
        const AABB &GetBounds() const
        {
            return buildBounds;
        }

        void SetCodeLease(std::shared_ptr<const void> lease)
        {
            codeLease = std::move(lease);
        }
        void SetMaxTiles(uint32_t value)
        {
            maxTiles = value;
        }
        uint32_t GetMaxTiles() const
        {
            return maxTiles;
        }
        void SetMaxSimplificationError(float value)
        {
            maxSimplificationError = value;
        }
        float GetMaxSimplificationError() const
        {
            return maxSimplificationError;
        }

        void        PrepareForBuild();
        NaviOctree *GetOctree() const
        {
            return octree.get();
        }

        virtual NaviQueryResult
        FindPath(const Vector3 &start, const Vector3 &end, const NaviQueryFilterPtr &filter, const NaviPathQueryParam &param) const = 0;

        // Synchronous callers keep inputs/filter unchanged and own out exclusively for the call.
        virtual NaviQueryResult QueryPath(
            const Vector3 &start, const Vector3 &end, const NaviQueryFilterPtr &filter, const NaviPathQueryParam &param, NaviPath &out) const = 0;

        virtual void BuildDebugGeometry(NaviDebugGeometry &out) const = 0;

        // Restores the mesh from a persisted asset payload (Tiled: per-tile blobs; Full: single blob).
        virtual bool LoadData(const NaviMeshData &data)                                                = 0;
        virtual bool ApplyBuild(const NaviMeshData &data, const std::vector<NaviMeshTileCoord> &tiles) = 0;
        virtual bool SaveFullData(std::vector<uint8_t> &out) const                                     = 0;

        // Adds a single persisted tile (streaming). Requires the mesh to be initialized with build params.
        virtual bool AddTile(const NaviMeshTilePayload &tile) = 0;

        // Ensures the coordinate is absent from mesh/cache; already absent succeeds on an initialized backend.
        virtual bool RemoveTile(const NaviMeshTileCoord &coord) = 0;

        // Initializes an empty mesh (nav mesh + tile cache) from build params so tiles can be streamed in.
        virtual bool PrepareStreaming(const NaviMeshBuildParams &params) = 0;

    protected:
        friend class NavigationSystem;
        virtual void OnAttachToWorld(World &world)
        {
        }
        virtual void OnDetachFromWorld(World &world)
        {
        }

        INavigationSystem *navSystem = nullptr;

        std::shared_ptr<const void> codeLease;
        uint32_t                    maxTiles               = 256;
        float                       maxSimplificationError = 1.3f;
        NaviAgentConfig             agentCfg;
        NaviMeshResolution          resolution;

        AABB                        buildBounds = {{-50.f, -50.f, -50.f}, {50.f, 50.f, 50.f}};
        std::unique_ptr<NaviOctree> octree;
    };

} // namespace sky::ai
