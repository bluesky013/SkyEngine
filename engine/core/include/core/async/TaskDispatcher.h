#pragma once

#include <core/async/ThreadPool.h>

namespace sky {

    enum class DispatchError : uint8_t { NONE, INVALID_STATE, INVALID_TASK, INVALID_EDGE, CYCLE, BUDGET_EXCEEDED };
    enum class DispatchState : uint8_t { SUCCEEDED, FAILED, SKIPPED };
    struct DispatchSubmission {
        DispatchError              error = DispatchError::NONE;
        std::future<DispatchState> completion;
        bool                       Accepted() const
        {
            return error == DispatchError::NONE && completion.valid();
        }
    };

    // Build/submit/clear belong to one owner; callbacks must not mutate the graph or wait on its pool.
    // Callbacks must not throw; use checked callbacks to report failures.
    class TaskDispatcher {
    public:
        using NodeIndex                         = uint32_t;
        static constexpr NodeIndex kInvalidNode = ~0u;
        TaskDispatcher()                        = default;
        ~TaskDispatcher();
        TaskDispatcher(const TaskDispatcher &)            = delete;
        TaskDispatcher &operator=(const TaskDispatcher &) = delete;

        NodeIndex                  CreateTask(ThreadTask &&func);
        NodeIndex                  CreateCheckedTask(std::function<bool(ThreadContext &)> func);
        DispatchError              DependsOn(NodeIndex child, NodeIndex parent);
        std::future<DispatchState> GetFuture(NodeIndex node);
        DispatchError              GetBuildError() const
        {
            return buildError;
        }
        // The pool must be exclusive to this batch; the total node budget bounds every worker queue.
        // Internal drain hook runs once after all node bodies exit, before the join future becomes ready.
        // It may reclaim graph storage; it must not wait on this submission or run user continuations inline.
        DispatchSubmission Submit(ThreadPool &pool, std::function<void(DispatchState)> onDrained = {});
        DispatchError      Clear();
        uint32_t           GetNodeCount() const
        {
            return static_cast<uint32_t>(nodes.size());
        }

    private:
        struct Node {
            ThreadTask                                   func;
            std::function<bool(ThreadContext &)>         checked;
            std::unique_ptr<std::promise<DispatchState>> promise;
            SmallVector<NodeIndex, 4>                    children;
            DispatchState                                result = DispatchState::SKIPPED;
        };
        NodeIndex                                    AddNode();
        void                                         ExecuteNode(NodeIndex index, ThreadContext &context);
        std::vector<Node>                            nodes;
        std::vector<uint32_t>                        pendingBuild;
        std::unique_ptr<std::atomic_uint32_t[]>      pending;
        std::unique_ptr<std::atomic_bool[]>          failedParents;
        std::atomic_uint32_t                         remaining{0};
        std::atomic_bool                             finished{true};
        bool                                         submitted  = false;
        DispatchError                                buildError = DispatchError::NONE;
        std::shared_ptr<std::promise<DispatchState>> batchPromise;
        std::function<void(DispatchState)>           drainCallback;
        ThreadPool                                  *pool = nullptr;
    };

} // namespace sky
