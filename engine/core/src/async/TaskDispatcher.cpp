#include <core/async/TaskDispatcher.h>
#include <core/platform/Platform.h>

namespace sky {

    TaskDispatcher::~TaskDispatcher()
    {
        SKY_ASSERT(finished.load(std::memory_order_acquire));
    }

    TaskDispatcher::NodeIndex TaskDispatcher::AddNode()
    {
        if (submitted || buildError != DispatchError::NONE) {
            if (buildError == DispatchError::NONE)
                buildError = DispatchError::INVALID_STATE;
            return kInvalidNode;
        }
        if (nodes.size() == kInvalidNode) {
            buildError = DispatchError::BUDGET_EXCEEDED;
            return kInvalidNode;
        }
        pendingBuild.push_back(0);
        nodes.emplace_back();
        return static_cast<NodeIndex>(nodes.size() - 1);
    }

    TaskDispatcher::NodeIndex TaskDispatcher::CreateTask(ThreadTask &&func)
    {
        if (!func) {
            buildError = DispatchError::INVALID_TASK;
            return kInvalidNode;
        }
        const auto index = AddNode();
        if (index != kInvalidNode)
            nodes[index].func = std::move(func);
        return index;
    }

    TaskDispatcher::NodeIndex TaskDispatcher::CreateCheckedTask(std::function<bool(ThreadContext &)> func)
    {
        if (!func) {
            buildError = DispatchError::INVALID_TASK;
            return kInvalidNode;
        }
        const auto index = AddNode();
        if (index != kInvalidNode)
            nodes[index].checked = std::move(func);
        return index;
    }

    DispatchError TaskDispatcher::DependsOn(NodeIndex child, NodeIndex parent)
    {
        if (submitted)
            return DispatchError::INVALID_STATE;
        if (buildError != DispatchError::NONE)
            return buildError;
        if (child >= nodes.size() || parent >= nodes.size() || child == parent || pendingBuild[child] == kInvalidNode) {
            return buildError = DispatchError::INVALID_EDGE;
        }
        nodes[parent].children.push_back(child);
        ++pendingBuild[child];
        return DispatchError::NONE;
    }

    std::future<DispatchState> TaskDispatcher::GetFuture(NodeIndex node)
    {
        if (submitted || node >= nodes.size() || nodes[node].promise) {
            buildError = DispatchError::INVALID_STATE;
            return {};
        }
        nodes[node].promise = std::make_unique<std::promise<DispatchState>>();
        return nodes[node].promise->get_future();
    }

    DispatchSubmission TaskDispatcher::Submit(ThreadPool &target, std::function<void(DispatchState)> onDrained)
    {
        if (submitted)
            return {DispatchError::INVALID_STATE};
        if (buildError != DispatchError::NONE)
            return {buildError};
        if (target.GetThreadCount() == 0 || nodes.size() > uint64_t{target.GetThreadCount()} * ThreadPool::GetLocalQueueCapacity()) {
            return {DispatchError::BUDGET_EXCEEDED};
        }
        std::vector<NodeIndex>     order;
        size_t                     rootCount = 0;
        std::future<DispatchState> future;
        auto                       counts = pendingBuild;
        order.reserve(nodes.size());
        for (NodeIndex i = 0; i < nodes.size(); ++i) {
            if (counts[i] == 0)
                order.push_back(i);
        }
        rootCount = order.size();
        for (size_t i = 0; i < order.size(); ++i) {
            for (auto child : nodes[order[i]].children) {
                if (--counts[child] == 0)
                    order.push_back(child);
            }
        }
        if (order.size() != nodes.size())
            return {DispatchError::CYCLE};
        pending       = std::make_unique<std::atomic_uint32_t[]>(nodes.size());
        failedParents = std::make_unique<std::atomic_bool[]>(nodes.size());
        batchPromise  = std::make_shared<std::promise<DispatchState>>();
        future        = batchPromise->get_future();
        for (NodeIndex i = 0; i < nodes.size(); ++i) {
            pending[i].store(pendingBuild[i], std::memory_order_relaxed);
            failedParents[i].store(false, std::memory_order_relaxed);
        }
        pool          = &target;
        drainCallback = std::move(onDrained);
        submitted     = true;
        remaining.store(static_cast<uint32_t>(nodes.size()), std::memory_order_relaxed);
        finished.store(nodes.empty(), std::memory_order_release);
        if (nodes.empty()) {
            auto completion = batchPromise;
            auto callback   = std::move(drainCallback);
            if (callback)
                callback(DispatchState::SUCCEEDED);
            completion->set_value(DispatchState::SUCCEEDED);
        }
        for (size_t i = 0; i < rootCount; ++i) {
            const auto root = order[i];
            target.Schedule([this, root](ThreadContext &context) { ExecuteNode(root, context); });
        }
        return {DispatchError::NONE, std::move(future)};
    }

    void TaskDispatcher::ExecuteNode(NodeIndex index, ThreadContext &context)
    {
        auto &node = nodes[index];
        if (!failedParents[index].load(std::memory_order_acquire)) {
            if (node.checked) {
                node.result = node.checked(context) ? DispatchState::SUCCEEDED : DispatchState::FAILED;
            } else {
                node.func(context);
                node.result = DispatchState::SUCCEEDED;
            }
        }
        if (node.promise)
            node.promise->set_value(node.result);
        for (auto child : node.children) {
            if (node.result != DispatchState::SUCCEEDED)
                failedParents[child].store(true, std::memory_order_release);
            if (pending[child].fetch_sub(1, std::memory_order_acq_rel) == 1) {
                pool->Schedule([this, child](ThreadContext &childContext) { ExecuteNode(child, childContext); });
            }
        }
        auto completion = batchPromise;
        if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            auto result = DispatchState::SUCCEEDED;
            for (const auto &nodeResult : nodes) {
                if (nodeResult.result != DispatchState::SUCCEEDED) {
                    result = DispatchState::FAILED;
                    break;
                }
            }
            auto callback = std::move(drainCallback);
            // Independent owners allow the drain hook to clear/destroy graph storage before notification.
            finished.store(true, std::memory_order_release);
            if (callback)
                callback(result);
            completion->set_value(result);
        }
    }

    DispatchError TaskDispatcher::Clear()
    {
        if (!finished.load(std::memory_order_acquire))
            return DispatchError::INVALID_STATE;
        nodes.clear();
        pendingBuild.clear();
        pending.reset();
        failedParents.reset();
        batchPromise.reset();
        pool       = nullptr;
        submitted  = false;
        buildError = DispatchError::NONE;
        return DispatchError::NONE;
    }

} // namespace sky
