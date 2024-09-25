//
// Created by blues on 2024/9/6.
//

#pragma once

#include <core/async/Task.h>

namespace sky::ai {

    class RecastNaviMapGenerator : public Task {
    public:
        RecastNaviMapGenerator() = default;
        ~RecastNaviMapGenerator() override = default;

    private:
        bool DoWork() override;
        void OnComplete(bool result) override;
    };

} // namespace sky::ai
