//
// Created on 2026/10/01.
//

#pragma once

#include <cstdint>

namespace sky::net {

    // Match the engine physics math mode. Only `Exact` is acceptable for deterministic lockstep.
    enum class DeterministicMathMode : uint8_t {
        Fast = 0,
        Exact
    };

    enum class LockstepResult : uint8_t {
        Ok = 0,
        NonDeterministicMathMode,
        RollbackUnavailable,
        RollbackDepthExceeded,
        InvalidArgument
    };

    struct LockstepConfig {
        uint32_t               tickRateHz = 30;
        uint32_t               inputDelayFrames = 2;
        uint32_t               rollbackMaxFrames = 8;
        uint32_t               hashCadenceFrames = 30;
        DeterministicMathMode  mathMode = DeterministicMathMode::Exact;
        // Enables local prediction with rollback reconciliation; otherwise the peer waits for the
        // authoritative frame before advancing.
        bool                   enablePrediction = false;
    };

    inline LockstepResult ValidateLockstepConfig(const LockstepConfig &config)
    {
        if (config.tickRateHz == 0) {
            return LockstepResult::InvalidArgument;
        }
        if (config.mathMode != DeterministicMathMode::Exact) {
            return LockstepResult::NonDeterministicMathMode;
        }
        return LockstepResult::Ok;
    }

} // namespace sky::net
