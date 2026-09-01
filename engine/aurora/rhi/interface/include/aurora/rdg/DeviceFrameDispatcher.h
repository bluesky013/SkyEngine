//
// Created on 2026/09/01.
//

#pragma once

#include <atomic>
#include <core/async/ThreadPool.h>
#include <core/template/SmallVector.h>
#include <cstdint>
#include <future>
#include <memory>
#include <vector>

namespace sky::aurora {

    // Single-threaded dependency graph scheduler backed by a ThreadPool.
    // Nodes are built with CreateTask / DependsOn on the calling thread, then
    // submitted as a batch. During execution children are read-only and nodes
    // are released as a whole, so no reference counting or locking is needed.
    class DeviceFrameDispatcher {
    public:
        using NodeIndex = uint32_t;

        static constexpr NodeIndex kInvalidNode = ~0u;

        DeviceFrameDispatcher() = default;
        ~DeviceFrameDispatcher();

        DeviceFrameDispatcher(const DeviceFrameDispatcher &)            = delete;
        DeviceFrameDispatcher &operator=(const DeviceFrameDispatcher &) = delete;

        // Build phase (single-threaded, before Submit).
        NodeIndex CreateTask(ThreadTask &&func);
        void      DependsOn(NodeIndex child, NodeIndex parent);

        // Returns a per-node future. Must be called during the build phase.
        std::future<void> GetFuture(NodeIndex node);

        // Submits the whole graph for execution. Returns a future that signals
        // completion of every node in the batch.
        std::future<void> Submit(ThreadPool &pool);

        // Resets for reuse. Must be called after the batch future is ready.
        void Clear();

        uint32_t GetNodeCount() const
        {
            return static_cast<uint32_t>(mNodes.size());
        }

    private:
        struct Node {
            ThreadTask                          func;
            std::unique_ptr<std::promise<void>> promise;
            SmallVector<NodeIndex, 4>           children;
        };

        void ExecuteNode(NodeIndex index, ThreadContext &ctx);

        std::vector<Node>                       mNodes;
        std::vector<uint32_t>                   mPendingBuild;
        std::unique_ptr<std::atomic_uint32_t[]> mPending;
        std::atomic_uint32_t                    mRemaining{0};
        std::shared_ptr<std::promise<void>>     mBatchPromise;
        ThreadPool                             *mPool = nullptr;
    };

} // namespace sky::aurora
