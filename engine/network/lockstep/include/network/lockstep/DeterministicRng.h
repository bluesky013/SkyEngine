//
// Created on 2026/10/01.
//

#pragma once

#include <cstdint>

namespace sky::net {

    // Deterministic RNG (xorshift64*) seeded from the tick. Simulation code MUST use this rather than a
    // global RNG so every peer produces the same draws.
    class DeterministicRng {
    public:
        void Seed(uint64_t seed) { state = seed != 0 ? seed : 0x9E3779B97F4A7C15ull; }

        uint32_t NextU32()
        {
            state ^= state >> 12;
            state ^= state << 25;
            state ^= state >> 27;
            return static_cast<uint32_t>((state * 0x2545F4914F6CDD1Dull) >> 32);
        }

        uint64_t State() const { return state; }

    private:
        uint64_t state = 0x9E3779B97F4A7C15ull;
    };

} // namespace sky::net
