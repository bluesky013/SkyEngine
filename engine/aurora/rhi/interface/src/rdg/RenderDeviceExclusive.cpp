//
// Created by Zach Lee on 2026/8/8.
//

#include <aurora/rdg/RenderDeviceExclusive.h>
#include <aurora/rhi/Instance.h>

namespace sky::aurora {
    void DeviceFrameContext::BeginFrame() noexcept
    {
    }

    void DeviceFrameContext::EndFrame() noexcept
    {
        mFrameAllocator.Reset();
        ++mFrameIndex;
    }

    RenderDeviceExclusive::RenderDeviceExclusive()
    {
        mDevice = Instance::Get()->GetDevice();
        mFrameContext = std::make_unique<DeviceFrameContext>();
    }

    RenderDeviceExclusive::~RenderDeviceExclusive()
    {
    }

    void RenderDeviceExclusive::BeginFrame() noexcept
    {

    }

    void RenderDeviceExclusive::EndFrame() noexcept
    {
    }

    void RenderDeviceExclusive::BeginViewport(RenderViewport* viewport) noexcept
    {
        SKY_ASSERT(mCurrentViewport != nullptr)
        mCurrentViewport = viewport;
    }

    void RenderDeviceExclusive::EndViewport() noexcept
    {
        mCurrentViewport = nullptr;
    }

} // namespace sky::aurora
