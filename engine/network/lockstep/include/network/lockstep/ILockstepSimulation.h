//
// Created on 2026/10/01.
//

#pragma once

#include <network/lockstep/LockstepTypes.h>

#include <cstdint>
#include <span>
#include <vector>

namespace sky::net {

    // The deterministic simulation driven by the lockstep host. Implementations MUST advance identically
    // for identical inputs on every peer (fixed step, deterministic iteration order, deterministic math).
    class ILockstepSimulation {
    public:
        virtual ~ILockstepSimulation() = default;

        // Advance exactly one fixed tick. `inputs` is indexed by player slot.
        virtual void Advance(LockstepFrame frame, const std::vector<std::vector<uint8_t>> &inputs) = 0;

        // Deterministic state capture/restore over stable ids.
        virtual void CaptureState(std::vector<uint8_t> &out) const = 0;
        virtual void RestoreState(std::span<const uint8_t> data) = 0;

        // Deterministic hash of the current state, used for divergence detection.
        virtual uint64_t StateHash() const = 0;
    };

} // namespace sky::net
