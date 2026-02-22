//
// Created by blues on 2025/2/21.
//

#pragma once

#include <chrono>
#include <deque>

namespace sky {

    class FpsCalculator {
    public:
        explicit FpsCalculator(float windowSize);
        ~FpsCalculator() = default;

        void Update();
        float GetFps() const;

    private:
        float size;
        std::deque<std::chrono::time_point<std::chrono::high_resolution_clock>> timestamps;
    };


} // namespace sky
