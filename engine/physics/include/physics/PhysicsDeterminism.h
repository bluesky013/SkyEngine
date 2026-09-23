//
// Created on 2026/09/23.
//

#pragma once

#include <cstdint>

namespace sky::phy {

    // Fast: native float, no determinism guarantee (delivered).
    // Exact: cross-platform bit-exact; reserved for a future deterministic backend.
    enum class PhysicsMathMode : uint8_t {
        Fast = 0,
        Exact
    };

    constexpr bool IsDeterministicMode(PhysicsMathMode mode)
    {
        return mode == PhysicsMathMode::Exact;
    }

    constexpr const char *ToString(PhysicsMathMode mode)
    {
        return mode == PhysicsMathMode::Exact ? "Exact" : "Fast";
    }

} // namespace sky::phy
