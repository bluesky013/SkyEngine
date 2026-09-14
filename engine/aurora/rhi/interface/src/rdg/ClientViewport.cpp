//
// Aurora ClientViewport implementation.
//

#include <aurora/rdg/ClientViewport.h>
#include <aurora/rhi/Device.h>
#include <core/logger/Logger.h>

namespace sky::aurora {

    ClientViewport::ClientViewport(Name name)
        : mName(name)
    {
    }

    bool ClientViewport::Init(Device *device, const SwapChain::Descriptor &desc)
    {
        if (device == nullptr) {
            return false;
        }

        mSwapChain = CounterPtr<SwapChain>(device->CreateSwapChain(desc));
        if (!mSwapChain) {
            return false;
        }

        const uint32_t count = mSwapChain->GetImageCount();
        mAcquireSemas.reserve(count);
        mRenderDoneSemas.reserve(count);

        Semaphore::Descriptor semaDesc{};
        semaDesc.type = SemaphoreType::BINARY;
        for (uint32_t i = 0; i < count; ++i) {
            mAcquireSemas.emplace_back(CounterPtr<Semaphore>(device->CreateSema(semaDesc)));
            mRenderDoneSemas.emplace_back(CounterPtr<Semaphore>(device->CreateSema(semaDesc)));
        }

        return true;
    }

    bool ClientViewport::Begin()
    {
        if (!mSwapChain) {
            mFrameValid = false;
            return false;
        }

        const SwapChainStatus s = mSwapChain->GetStatus();
        if (s == SwapChainStatus::LOST) {
            mFrameValid = false;
            return false;
        }
        if (s == SwapChainStatus::OUT_OF_DATE) {
            const Extent2D size = mSwapChain->GetSurfaceSize();
            if (size.width == 0 || size.height == 0) {
                mFrameValid = false; // minimized
                return false;
            }
            mSwapChain->Resize(size.width, size.height);
            if (mSwapChain->GetStatus() == SwapChainStatus::LOST) {
                mFrameValid = false;
                return false;
            }
        }

        mFrameValid = true;
        return true;
    }

    bool ClientViewport::Acquire()
    {
        if (!mSwapChain || !mFrameValid) {
            return false;
        }

        mCurrentSlot = mFrameSlot % static_cast<uint32_t>(mAcquireSemas.size());
        const uint32_t index = mSwapChain->AcquireNextImage(
            mAcquireSemas[mCurrentSlot].Get(), nullptr, UINT64_MAX);
        if (index == INVALID_INDEX) {
            mFrameValid = false;
            return false;
        }

        mImageIndex = index;
        ++mFrameSlot;
        return true;
    }

    void ClientViewport::Release()
    {
        if (!mSwapChain || !mFrameValid) {
            return;
        }

        Semaphore *sema = mRenderDoneSemas[mCurrentSlot].Get();
        mSwapChain->Present(mImageIndex, 1, &sema);
    }

    Image *ClientViewport::GetBackbuffer() const
    {
        if (!mSwapChain || !mFrameValid) {
            return nullptr;
        }
        return mSwapChain->GetImage(mImageIndex);
    }

    PixelFormat ClientViewport::GetFormat() const
    {
        return mSwapChain ? mSwapChain->GetFormat() : PixelFormat::UNDEFINED;
    }

    Extent2D ClientViewport::GetExtent() const
    {
        return mSwapChain ? mSwapChain->GetExtent() : Extent2D{0, 0};
    }

    const Name &ClientViewport::GetName() const
    {
        return mName;
    }

    Semaphore *ClientViewport::GetAcquireSemaphore() const
    {
        return mCurrentSlot < mAcquireSemas.size() ? mAcquireSemas[mCurrentSlot].Get() : nullptr;
    }

    Semaphore *ClientViewport::GetRenderDoneSemaphore() const
    {
        return mCurrentSlot < mRenderDoneSemas.size() ? mRenderDoneSemas[mCurrentSlot].Get() : nullptr;
    }

} // namespace sky::aurora
