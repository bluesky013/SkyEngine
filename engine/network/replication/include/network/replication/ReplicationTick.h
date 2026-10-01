//
// Created on 2026/09/25.
//

#pragma once

#include <cstdint>

namespace sky::net {

    // Fixed network tick with a bounded accumulator on real time, independent of the render frame rate.
    class ReplicationTick {
    public:
        explicit ReplicationTick(uint32_t rateHz, uint32_t maxCatchUp = 4)
            : step(rateHz == 0 ? 1.0 : 1.0 / static_cast<double>(rateHz)), maxCatchUp(maxCatchUp)
        {
        }

        // Returns the number of fixed ticks to run for this frame.
        uint32_t Advance(double deltaSeconds)
        {
            accumulator += deltaSeconds;

            uint32_t ticks = 0;
            while (accumulator >= step && ticks < maxCatchUp) {
                accumulator -= step;
                ++ticks;
                ++totalTicks;
            }
            if (ticks >= maxCatchUp) {
                accumulator = 0.0;   // drop the excess backlog to avoid a tick spiral
            }
            return ticks;
        }

        uint64_t TotalTicks() const { return totalTicks; }
        double   StepSeconds() const { return step; }

    private:
        double   step;
        double   accumulator = 0.0;
        uint32_t maxCatchUp;
        uint64_t totalTicks = 0;
    };

} // namespace sky::net
