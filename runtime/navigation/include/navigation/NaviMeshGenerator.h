//
// Created by blues on 2024/10/11.
//

#pragma once

#include <core/async/Task.h>
#include <navigation/NaviMesh.h>

namespace sky::ai {

    class NaviMeshGenerator : public Task {
    public:
        NaviMeshGenerator() = default;
        ~NaviMeshGenerator() override = default;

        virtual void Setup(const CounterPtr<NaviMesh> &naviMesh) = 0;
    };

} // namespace sky::ai
