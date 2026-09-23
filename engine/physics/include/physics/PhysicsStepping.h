//
// Created on 2026/09/23.
//

#pragma once

#include <cstdint>

namespace sky::phy {

    struct PhysicsStepConfig {
        float    fixedDelta  = 1.f / 60.f;
        uint32_t maxSubSteps = 4;
    };

    // Pure fixed-timestep driver. Owns the accumulator and frame counter; the caller runs the returned
    // number of fixed steps and reads the interpolation alpha for rendering.
    class PhysicsStepper {
    public:
        void SetConfig(const PhysicsStepConfig &cfg);
        const PhysicsStepConfig &GetConfig() const { return config; }

        // Returns how many fixed steps should run for this frame delta, capped by maxSubSteps.
        uint32_t Advance(float frameDelta);

        // Alpha in [0, 1) between the previous and current simulated state, for rendering.
        float GetInterpolationAlpha() const;

        uint64_t GetFrame() const { return frame; }

        void Reset();

    private:
        PhysicsStepConfig config;
        float             accumulator = 0.f;
        uint64_t          frame       = 0;
    };

} // namespace sky::phy
