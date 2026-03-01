//
// Created by blues on 2025/2/21.
//

#pragma once

#include <cstdint>

namespace sky::rdg {

    struct RenderGraphData {
        uint32_t triangleData;
        uint32_t drawCall;

        void Reset()
        {
            triangleData = 0;
            drawCall = 0;
        }
    };

} // namespace sky
