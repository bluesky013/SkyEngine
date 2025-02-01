//
// Created by blues on 2024/11/3.
//

#pragma once

#include <core/math/Vector3.h>

namespace sky::ai {

    struct NaviLocation {
        Vector3 position;
    };

    class NaviPath {
    public:
        NaviPath() = default;
        ~NaviPath() = default;

    private:
        std::vector<NaviLocation> pathPoints;
    };

} // namespace sky::ai
