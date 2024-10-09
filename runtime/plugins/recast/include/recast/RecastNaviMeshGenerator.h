//
// Created by blues on 2024/9/6.
//

#pragma once

#include <core/async/Task.h>
#include <Recast.h>
#include <DetourTileCache.h>
#include <recast/RecastNaviMesh.h>

namespace sky::ai {

    class RecastNaviMeshGenerator : public Task {
    public:
        explicit RecastNaviMeshGenerator(const CounterPtr<RecastNaviMesh>& mesh);
        ~RecastNaviMeshGenerator() override = default;

    private:
        bool DoWork() override;
        void OnComplete(bool result) override;

        CounterPtr<RecastNaviMesh> navMesh;
    };

} // namespace sky::ai
