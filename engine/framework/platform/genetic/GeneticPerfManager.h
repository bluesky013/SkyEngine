//
// Created by Zach Lee on 2023/5/9.
//

#pragma once

#include <framework/performance/AdaptivePerfManager.h>

namespace sky {

    class GeneticPerfManager : public AdaptivePerfManager {
    public:
        GeneticPerfManager() = default;
        ~GeneticPerfManager() = default;
    };

} // namespace sky