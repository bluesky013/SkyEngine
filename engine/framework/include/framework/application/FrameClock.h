#pragma once

#include <cstdint>

namespace sky {

    // Per-application timing. Reset after suspension so background time does
    // not become a simulation step on the first resumed frame.
    struct FrameClock {
        uint64_t previousCounter   = 0;
        uint64_t previousFrequency = 0;
        uint64_t previousEpoch     = 0;
        bool     hasSample         = false;

        void Reset()
        {
            hasSample = false;
        }

        float Sample(uint64_t counter, uint64_t frequency, uint64_t epoch = 0)
        {
            constexpr float INITIAL_DELTA = 1.0f / 60.0f;
            if (frequency == 0) {
                Reset();
                return INITIAL_DELTA;
            }
            const bool  continuous = hasSample && epoch == previousEpoch && frequency == previousFrequency && counter >= previousCounter;
            const float delta =
                continuous ? static_cast<float>(static_cast<double>(counter - previousCounter) / static_cast<double>(frequency)) : INITIAL_DELTA;
            previousCounter   = counter;
            previousFrequency = frequency;
            previousEpoch     = epoch;
            hasSample         = true;
            return delta;
        }
    };

} // namespace sky
