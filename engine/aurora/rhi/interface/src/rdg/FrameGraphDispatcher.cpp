//
// Created on 2026/09/01.
//

#include <aurora/rdg/FrameGraphDispatcher.h>

#include <core/platform/Platform.h>

namespace sky::aurora {

    FrameGraphDispatcher::~FrameGraphDispatcher()
    {
        SKY_ASSERT(mRemaining.load(std::memory_order_relaxed) == 0);
    }

    FrameGraphDispatcher::NodeIndex FrameGraphDispatcher::CreateTask(ThreadTask &&func)
    {
        NodeIndex index = static_cast<NodeIndex>(mNodes.size());
        mNodes.emplace_back();
        mNodes.back().func = std::move(func);
        mPendingBuild.emplace_back(0);
        return index;
    }

    void FrameGraphDispatcher::DependsOn(NodeIndex child, NodeIndex parent)
    {
        SKY_ASSERT(child < mNodes.size() && parent < mNodes.size());
        ++mPendingBuild[child];
        mNodes[parent].children.push_back(child);
    }

    std::future<void> FrameGraphDispatcher::GetFuture(NodeIndex node)
    {
        SKY_ASSERT(node < mNodes.size());
        if (mNodes[node].promise == nullptr) {
            mNodes[node].promise = std::make_unique<std::promise<void>>();
        }
        return mNodes[node].promise->get_future();
    }

    std::future<void> FrameGraphDispatcher::Submit(ThreadPool &pool)
    {
        mPool = &pool;
        mRemaining.store(static_cast<uint32_t>(mNodes.size()), std::memory_order_relaxed);
        mBatchPromise = std::make_shared<std::promise<void>>();

        // Freeze the parent counts into an atomic array. The build phase is
        // single-threaded, so mPendingBuild is a plain vector; the atomic array
        // is what workers decrement during execution.
        mPending = std::make_unique<std::atomic_uint32_t[]>(mNodes.size());
        for (size_t i = 0; i < mNodes.size(); ++i) {
            mPending[i].store(mPendingBuild[i], std::memory_order_relaxed);
        }

        // Collect roots before scheduling any node: once workers start, they
        // decrement pending counts concurrently and would corrupt a scan that
        // enqueues as it goes.
        std::vector<NodeIndex> roots;
        roots.reserve(mNodes.size());
        for (NodeIndex i = 0; i < mNodes.size(); ++i) {
            if (mPending[i].load(std::memory_order_relaxed) == 0) {
                roots.push_back(i);
            }
        }

        for (NodeIndex root : roots) {
            pool.Schedule([this, root](ThreadContext &ctx) { ExecuteNode(root, ctx); });
        }

        if (mNodes.empty()) {
            mBatchPromise->set_value();
        }

        return mBatchPromise->get_future();
    }

    void FrameGraphDispatcher::Clear()
    {
        SKY_ASSERT(mRemaining.load(std::memory_order_relaxed) == 0);
        mNodes.clear();
        mPendingBuild.clear();
        mPending.reset();
        mRemaining.store(0, std::memory_order_relaxed);
        mBatchPromise.reset();
        mPool = nullptr;
    }

    void FrameGraphDispatcher::ExecuteNode(NodeIndex index, ThreadContext &ctx)
    {
        Node &node = mNodes[index];
        node.func(ctx);

        if (node.promise != nullptr) {
            node.promise->set_value();
        }

        for (NodeIndex child : node.children) {
            if (mPending[child].fetch_sub(1, std::memory_order_acq_rel) == 1) {
                mPool->Schedule([this, child](ThreadContext &childCtx) { ExecuteNode(child, childCtx); });
            }
        }

        if (mRemaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            mBatchPromise->set_value();
        }
    }

} // namespace sky::aurora
