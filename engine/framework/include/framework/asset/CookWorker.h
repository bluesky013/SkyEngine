//
// Created by blues on 2026/10/2.
//

#pragma once

#include <string>
#include <vector>
#include <core/util/Uuid.h>

namespace sky {

    // Batch cook driver used by the builder-side AssetTool background worker.
    // The in-process implementation drains cooks on the cook/asset pools; an out-of-process
    // host wraps this over IPC.
    class CookWorker {
    public:
        struct Job {
            Uuid uuid;
            std::string target;
        };

        // Cook a batch of explicit (uuid, target) jobs; blocks until all scheduled cooks drain.
        void CookBatch(const std::vector<Job> &jobs);

        // Cook every registered source asset for its configured targets; blocks until drained.
        void CookAll();
    };

} // namespace sky
