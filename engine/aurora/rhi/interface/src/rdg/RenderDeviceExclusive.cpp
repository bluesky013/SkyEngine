//
// Created by Zach Lee on 2026/8/8.
//

#include <aurora/rdg/RenderDeviceExclusive.h>
#include <aurora/rhi/Device.h>
#include <aurora/rhi/Instance.h>

namespace sky::aurora {

    void DeviceFrameContext::InitFences(Device *device)
    {
        if (device == nullptr || mInflightNum == 0) {
            return;
        }

        Fence::Descriptor desc{}; // createSignaled = true
        mFences.clear();
        mFences.reserve(mInflightNum);
        for (uint32_t i = 0; i < mInflightNum; ++i) {
            mFences.emplace_back(device->CreateFence(desc));
        }
    }

    Fence *DeviceFrameContext::GetFrameFence() const noexcept
    {
        if (mFences.empty()) {
            return nullptr;
        }
        return mFences[mFrameIndex % mFences.size()].Get();
    }

    void DeviceFrameContext::BeginFrame() noexcept
    {
        if (!mFences.empty()) {
            Fence *fence = mFences[mFrameIndex % mFences.size()].Get();
            fence->Wait();
            fence->Reset();
        }
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
        SKY_ASSERT(mCurrentViewport == nullptr)
        mCurrentViewport = viewport;
    }

    void RenderDeviceExclusive::EndViewport() noexcept
    {
        mCurrentViewport = nullptr;
    }

} // namespace sky::aurora
