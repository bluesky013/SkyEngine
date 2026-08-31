//
// Created by Zach Lee on 2026/8/8.
//

#pragma once

#include "core/platform/Platform.h"

#include <core/async/ThreadPool.h>
#include <core/environment/Singleton.h>

namespace sky::aurora {

    class Device;
    class RenderViewport;

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

        ThreadPool *GetParallelContext() const
        {
            return mThreadPool.get();
        }

    protected:
        uint32_t                    mFrameIndex  = 0;
        uint32_t                    mInflightNum = 0;
        uint32_t                    mParallelNum = 0;
        std::unique_ptr<ThreadPool> mThreadPool;
    };

    class RenderDeviceExclusive : public Singleton<RenderDeviceExclusive> {
    public:
        RenderDeviceExclusive();
        ~RenderDeviceExclusive() override;

        FORCEINLINE Device *GetDevice() const noexcept
        {
            return mDevice;
        }

        void BeginFrame() noexcept;
        void EndFrame() noexcept;

        void BeginViewport(RenderViewport *viewport) noexcept;
        void EndViewport() noexcept;

    private:
        Device *mDevice = nullptr;

        RenderViewport *mCurrentViewport = nullptr;

        std::unique_ptr<DeviceFrameContext> mFrameContext;
    };

} // namespace sky::aurora
