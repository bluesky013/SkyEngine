#pragma once

#include <aurora/rhi/Fence.h>
#include <core/async/ThreadPool.h>
#include <core/memory/FrameAllocator.h>
#include <memory>
#include <vector>

namespace sky::aurora {

    class Device;
    class IDeviceFrameExecution;

    struct DeviceFrameContextInitInfo {
        uint32_t inflightNum = 2;
        uint32_t parallelNum = 1;
    };

    class DeviceFrameContext {
    public:
        DeviceFrameContext()          = default;
        virtual ~DeviceFrameContext() = default;

        virtual void BeginFrame() noexcept;
        virtual void EndFrame() noexcept;

        // Context-owned execution shares this slot ring/pool; nullptr means legacy-only.
        // Never mix this protocol with BeginFrame/EndFrame on the same context.
        virtual IDeviceFrameExecution *GetFrameExecution() noexcept
        {
            return nullptr;
        }

        ThreadPool *GetParallelContext() const
        {
            return mThreadPool.get();
        }
        FrameAllocator &GetFrameAllocator() noexcept
        {
            return mFrameAllocator;
        }
        Fence *GetFrameFence() const noexcept;

    protected:
        void InitFences(Device *device);

        uint32_t                    mFrameIndex  = 0;
        uint32_t                    mInflightNum = 0;
        uint32_t                    mParallelNum = 0;
        std::unique_ptr<ThreadPool> mThreadPool;
        FrameAllocator              mFrameAllocator;
        std::vector<FencePtr>       mFences;
    };

} // namespace sky::aurora
