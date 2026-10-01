//
// Created on 2026/09/25.
//

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace sky::net {

    // Holds the two most recent snapshots received for a record and reports an interpolation alpha in
    // [0, 1]. The payloads are opaque; a typed adapter lerps its own fields using the alpha.
    class SnapshotInterpolationBuffer {
    public:
        void Push(double time, std::span<const uint8_t> payload)
        {
            if (hasCurrent) {
                previous     = std::move(current);
                previousTime = currentTime;
                hasPrevious  = true;
            }
            current.assign(payload.begin(), payload.end());
            currentTime = time;
            hasCurrent  = true;
        }

        bool HasPrevious() const { return hasPrevious; }
        bool HasCurrent() const { return hasCurrent; }

        // alpha = (now - renderDelay - previousTime) / (currentTime - previousTime), clamped to [0, 1].
        double Alpha(double now, double renderDelay = 0.0) const
        {
            if (!hasPrevious || currentTime <= previousTime) {
                return 1.0;
            }
            const double target = now - renderDelay;
            double alpha = (target - previousTime) / (currentTime - previousTime);
            if (alpha < 0.0) {
                alpha = 0.0;
            }
            if (alpha > 1.0) {
                alpha = 1.0;
            }
            return alpha;
        }

        std::span<const uint8_t> Previous() const { return previous; }
        std::span<const uint8_t> Current() const { return current; }

    private:
        std::vector<uint8_t> previous;
        std::vector<uint8_t> current;
        double               previousTime = 0.0;
        double               currentTime = 0.0;
        bool                 hasPrevious = false;
        bool                 hasCurrent = false;
    };

} // namespace sky::net
