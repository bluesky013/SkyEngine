#pragma once

#include "prepare/FrozenPrepareExecutor.h"
#include <core/async/Completion.h>

namespace sky::aurora {

    using PrepareKernel = std::function<void(const FrozenPrepareContext &)>;
    // Opt-in promises element-wise work: each chunk owns its rows; shared resource writes are rejected.
    struct ElementWisePrepare {
        SystemId system = 0;
        SceneId  scene;
        size_t   rowsPerChunk = 256;
        size_t   scratchBytes = 0;
    };
    struct ScenePrepareInit {
        uint32_t workers             = 2;
        uint32_t maxParallelNodes    = 256;
        size_t   maxBoundRows        = 1024 * 1024;
        size_t   maxTaskNodes        = 4096;
        size_t   maxScratchBytes     = 16 * 1024 * 1024;
        size_t   maxOutputRecords    = 65536;
        size_t   maxOutputBytes      = 16 * 1024 * 1024;
        size_t   maxDeferredCommands = 65536;
    };

    // Completion proves kernel drain; escaped views and continuation captures independently pin frozen storage.
    class PrepareJobGroup {
    public:
        ~PrepareJobGroup();
        PrepareJobGroup(const PrepareJobGroup &)                = delete;
        PrepareJobGroup     &operator=(const PrepareJobGroup &) = delete;
        CompletionState      Poll();
        const PrepareResult &Wait();
        CompletionToken      GetCompletion() const;
        void                 RequestCancel();
        bool                 UsedSerialFallback() const;

    private:
        friend class ScenePrepareScheduler;
        struct State;
        explicit PrepareJobGroup(std::shared_ptr<State> state);
        std::shared_ptr<State> state;
    };

    struct PrepareSubmitResult {
        std::shared_ptr<PrepareJobGroup> group;
        std::optional<RendererError>     error;
        bool                             Accepted() const
        {
            return group != nullptr && !error;
        }
    };
    struct PrepareSchedulerCreateResult;

    struct CpuPrepareProgram {
        std::vector<SystemDesc>                           systems;
        std::function<void(const FrozenPrepareContext &)> kernel;
        std::vector<ElementWisePrepare>                   partitions;
        std::vector<PrepareOutputConfig>                  outputs;
        std::function<void(const PrepareResult &)>        completed;
    };

    // One active group owns the private pool budget; kernels may run concurrently and must not wait on that pool.
    class ScenePrepareScheduler {
    public:
        static PrepareSchedulerCreateResult Create(const ScenePrepareInit &info = {});
        ~ScenePrepareScheduler();
        ScenePrepareScheduler(const ScenePrepareScheduler &)            = delete;
        ScenePrepareScheduler &operator=(const ScenePrepareScheduler &) = delete;
        PrepareSubmitResult    Submit(const FrozenCpuFrame            &frame,
                                      std::vector<SystemDesc>          systems,
                                      PrepareKernel                    kernel,
                                      std::vector<ElementWisePrepare>  partitions = {},
                                      std::vector<PrepareOutputConfig> outputs    = {});

    private:
        struct State;
        explicit ScenePrepareScheduler(std::unique_ptr<State> state);
        std::unique_ptr<State> state;
    };

    struct PrepareSchedulerCreateResult {
        std::unique_ptr<ScenePrepareScheduler> scheduler;
        std::optional<RendererError>           error;
    };

} // namespace sky::aurora
