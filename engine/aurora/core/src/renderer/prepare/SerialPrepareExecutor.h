#pragma once

#include "prepare/PrepareProducts.h"
#include <core/ecs/Dependency.h>
#include <functional>
#include <memory>

namespace sky::aurora {

    enum class PrepareNodeState : uint8_t { SUCCEEDED, FAILED, SKIPPED };

    struct PrepareNodeResult {
        SystemId         system = 0;
        PrepareNodeState state  = PrepareNodeState::SKIPPED;
        std::string      diagnostic;
    };

    struct PrepareResult {
        std::vector<PrepareNodeResult>         nodes;
        std::string                            error;
        std::shared_ptr<const PrepareProducts> products;
        bool                                   cancelled = false;
        bool                                   Succeeded() const;
    };

    using SerialPrepareResult = PrepareResult;

    // Runs on the caller's thread; failed dependencies skip consumers, independent nodes still complete.
    SerialPrepareResult ExecuteSerialPrepare(const DependencyPlan &plan, const std::function<void(SystemId)> &kernel);
    PrepareResult       ExecuteSerialPrepareChecked(const DependencyPlan &plan, const std::function<PrepareNodeResult(SystemId)> &kernel);

} // namespace sky::aurora
