//
// Created by Zach Lee on 2026/8/8.
//

#pragma once

#include "core/platform/Platform.h"

#include <aurora/rdg/DeviceFrameContext.h>
#include <core/environment/Singleton.h>

namespace sky::aurora {

    class Device;
    class RenderViewport;

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

        FrameAllocator &GetFrameAllocator() noexcept
        {
            return mFrameContext->GetFrameAllocator();
        }

    private:
        Device *mDevice = nullptr;

        RenderViewport *mCurrentViewport = nullptr;

        std::unique_ptr<DeviceFrameContext> mFrameContext;
    };

} // namespace sky::aurora
