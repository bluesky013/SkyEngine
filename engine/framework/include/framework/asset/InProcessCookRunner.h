//
// Created by blues on 2026/10/3.
//

#pragma once

#include <framework/asset/ICookRunner.h>

namespace sky {

    // Runs cooks on the engine's asset/cook pools via AssetBuilderManager, which
    // raises IAssetEvent::OnAssetBuildFinished on completion.
    class InProcessCookRunner : public ICookRunner {
    public:
        InProcessCookRunner() = default;
        ~InProcessCookRunner() override = default;

        void SetCompletion(CookCompletion handler) override;
        bool Request(const CookJob &job) override;
        void Drain() override;

    private:
        // Assigned once during setup (before any Request); read on the cook-pool thread.
        CookCompletion completion;
    };

} // namespace sky
