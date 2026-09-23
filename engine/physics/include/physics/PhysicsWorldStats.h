//
// Created on 2026/09/23.
//

#pragma once

#include <cstdint>

namespace sky::phy {

    struct PhysicsWorldStats {
        uint32_t bodyCount       = 0;
        uint32_t activeBodyCount = 0;
        uint32_t characterCount  = 0;
        uint32_t constraintCount = 0;
        uint32_t contactCount    = 0;

        uint64_t stepCount            = 0;
        double   lastStepMilliseconds = 0.0;
    };

} // namespace sky::phy
