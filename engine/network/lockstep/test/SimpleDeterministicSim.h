//
// Created on 2026/10/01.
//

#pragma once

#include <network/lockstep/ILockstepSimulation.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace sky::net::test {

    // A tiny deterministic simulation for lockstep tests: entity values integrated from per-player inputs.
    class SimpleDeterministicSim : public ILockstepSimulation {
    public:
        explicit SimpleDeterministicSim(uint32_t entityCount = 4) : values(entityCount, 0) {}

        void Advance(LockstepFrame frame, const std::vector<std::vector<uint8_t>> &inputs) override
        {
            uint32_t sum = 0;
            for (const auto &input : inputs) {
                for (uint8_t byte : input) {
                    sum += byte;
                }
            }
            if (divergenceFramesRemaining > 0) {
                sum += 1000;   // inject a one-off divergence for desync tests
                --divergenceFramesRemaining;
            }
            for (size_t i = 0; i < values.size(); ++i) {
                values[i] = values[i] * 3u + sum + static_cast<uint32_t>(frame) + static_cast<uint32_t>(i);
            }
            lastFrame = frame;
        }

        void CaptureState(std::vector<uint8_t> &out) const override
        {
            out.resize(values.size() * sizeof(uint32_t));
            std::memcpy(out.data(), values.data(), out.size());
        }

        void RestoreState(std::span<const uint8_t> data) override
        {
            if (data.size() != values.size() * sizeof(uint32_t)) {
                return;
            }
            std::memcpy(values.data(), data.data(), data.size());
        }

        uint64_t StateHash() const override
        {
            uint64_t hash = 1469598103934665603ull;
            for (uint32_t value : values) {
                hash ^= value;
                hash *= 1099511628211ull;
            }
            return hash;
        }

        uint32_t Value(size_t index) const { return values[index]; }

        void InjectDivergence(uint32_t frames) { divergenceFramesRemaining = frames; }

    private:
        std::vector<uint32_t> values;
        LockstepFrame         lastFrame = 0;
        uint32_t              divergenceFramesRemaining = 0;
    };

} // namespace sky::net::test
