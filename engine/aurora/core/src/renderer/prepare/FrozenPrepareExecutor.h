#pragma once

#include "prepare/SerialPrepareExecutor.h"
#include "scene/FrozenCpuFrame.h"

namespace sky::aurora {

    class FrozenPrepareExecutor {
    public:
        static SerialPrepareResult
        Run(const FrozenCpuFrame &frame, const DependencyPlan &plan, const std::function<void(const FrozenPrepareContext &)> &kernel);

    private:
        friend class ScenePrepareScheduler;
        friend class PrepareJobGroup;
        static std::optional<RendererError>
        Bind(const FrozenCpuFrame &frame, const DependencyPlan &plan, size_t maxRows, std::vector<FrozenPrepareContext> &contexts);
        static PrepareNodeResult
        Invoke(FrozenPrepareContext &context, const std::function<void(const FrozenPrepareContext &)> &kernel, bool publish = true);
        static std::optional<size_t> PartitionRows(const FrozenPrepareContext &context, SceneId scene);
        static FrozenPrepareContext  MakeChunk(const FrozenPrepareContext &context, PrepareChunkRange range, size_t scratchBytes);
        static void                  Publish(FrozenPrepareContext &context) noexcept;
        static void                  ConfigureOutputs(FrozenPrepareContext &context, const PrepareOutputConfig &config);
        static void                  MergeOutputs(const FrozenPrepareContext &context, PrepareProducts &result);
    };

} // namespace sky::aurora
