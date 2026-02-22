//
// Created by blues on 2024/11/13.
//

#pragma once

#include <core/async/Task.h>

namespace sky {

    class TerrainGenerator : public Task {
    public:
        TerrainGenerator() = default;
        ~TerrainGenerator() = default;

        struct Config {
            uint32_t maxExt = 16 * 1024;
            uint32_t minExt = 64;
        };

        void Setup(const Config &cfg);
    private:
        bool DoWork() override;

        Config config;
    };

} // namespace sky
