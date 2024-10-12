//
// Created by blues on 2024/9/6.
//

#pragma once

#include <Recast.h>
#include <DetourTileCache.h>
#include <recast/RecastNaviMesh.h>

namespace sky::ai {

    class RecastNaviMeshGenerator : public NaviMeshGenerator {
    public:
        RecastNaviMeshGenerator() = default;
        ~RecastNaviMeshGenerator() override = default;

        void Setup(const CounterPtr<NaviMesh> &naviMesh) override;
    private:
        bool DoWork() override;
        void OnComplete(bool result) override;

        CounterPtr<RecastNaviMesh> navMesh;
        rcConfig config;
    };

} // namespace sky::ai
