//
// Created by blues on 2024/9/1.
//

#pragma once

#include <core/math/Vector3.h>
#include <navigation/NaviMesh.h>
#include <recast/RecastDebugDraw.h>
#include <render/RenderPrimitive.h>

class dtNavMesh;
class dtNavMeshQuery;

namespace sky {
    class World;
} // namespace sky

namespace sky::ai {

    struct RecastNaviMapConfig {
        uint32_t maxTiles   = 256;
        uint32_t maxPolys   = 16384;
    };

    struct RecastTile {
        int32_t x;
        int32_t y;
    };

    struct RecastNavData : RefObject {
        RecastNavData() = default;
        ~RecastNavData() override;

        uint8_t *data = nullptr;
        uint32_t size = 0;
    };
    using RecastNavDataPtr = CounterPtr<RecastNavData>;

    struct RecastMeshTileData {
        RecastNavDataPtr navData;
    };

    class RecastNaviMesh : public NaviMesh {
    public:
        RecastNaviMesh();
        ~RecastNaviMesh() override;

        bool BuildNavMesh(const RecastNaviMapConfig &config);
        void BuildNavQuery();
        void BuildDebugDraw();
        void SetTechnique(const RDGfxTechPtr &tech);

        dtNavMesh* GetNavMesh() const { return navMesh; }

        NaviQueryResult FindPath(const Vector3 &start, const Vector3 &end, const NaviQueryFilterPtr& filter, const NaviPathQueryParam &param) const override;
    private:
        void ResetNavMesh();

        void OnAttachToWorld(World &world) override;
        void OnDetachFromWorld(World &world) override;
        dtNavMesh *navMesh = nullptr;
        dtNavMeshQuery* navQuery = nullptr;

        std::unique_ptr<RenderPrimitive> primitive;
        std::unique_ptr<DebugRenderer> debugDraw;
    };
} // namespace sky::ai