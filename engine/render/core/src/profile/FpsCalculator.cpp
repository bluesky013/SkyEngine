//
// Created by blues on 2025/2/21.
//

#include <render/profile/FpsCalculator.h>

namespace sky {

    FpsCalculator::FpsCalculator(float windowsize)
        : size(windowsize)
    {
    }

    float FpsCalculator::GetFps() const
    {
        if (timestamps.size() < 2) {
            return 0.0;
        }

        const auto& oldest = timestamps.front();
        const auto& newest = timestamps.back();
        const auto duration = std::chrono::duration<float>(newest - oldest).count();

        return duration > 0.0f ? static_cast<float>(timestamps.size() - 1) / duration : 0.0f;
    }

    void FpsCalculator::Update()
    {
        const auto now = std::chrono::high_resolution_clock::now();
        timestamps.push_back(now);

        const auto expired = now - std::chrono::duration<double>(size);
        while (!timestamps.empty() && timestamps.front() < expired) {
            timestamps.pop_front();
        }
    }

} // namespace sky