//
// Created by blues on 2024/11/3.
//

#pragma once

#include <navigation/NaviMesh.h>

#include <core/math/Vector3.h>

#include <cstdint>
#include <vector>

namespace sky::ai {

    struct NaviLocation {
        Vector3 position;
    };

    // Query result: ordered world-space points plus their straight-path flags.
    struct NaviPath {
        NaviQueryResult      result = NaviQueryResult::FAILED;
        std::vector<Vector3> points;
        std::vector<uint8_t> flags;

        bool IsValid() const { return result == NaviQueryResult::SUCCESS; }

        void Reset()
        {
            result = NaviQueryResult::FAILED;
            points.clear();
            flags.clear();
        }
    };

} // namespace sky::ai
