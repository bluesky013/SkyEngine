//
// Created on 2026/08/31.
//

#include <D3D12Device.h>
#include <rdg/D3D12DeviceFrameContext.h>

namespace sky::aurora {

    D3D12DeviceFrameContext::D3D12DeviceFrameContext(D3D12Device *device, const DeviceFrameContextInitInfo &info) : mDevice(device)
    {
        mInflightNum = info.inflightNum;
        mParallelNum = info.parallelNum;

        mPool.reset(device->CreateCommandPool(QueueType::GRAPHICS));
        mD3D12Pool = static_cast<D3D12CommandPool *>(mPool.get());

        mBuffers.resize(info.inflightNum);
        for (uint32_t i = 0; i < info.inflightNum; ++i) {
            mBuffers[i] = mD3D12Pool->Allocate();
        }

        if (info.parallelNum > 1) {
            mThreadPool =
                std::make_unique<ThreadPool>(info.parallelNum, [device](uint32_t) { return new D3D12Context(*device, QueueType::GRAPHICS); });

            mParallelBuffers.resize(info.parallelNum);
            for (uint32_t w = 0; w < info.parallelNum; ++w) {
                auto *ctx = static_cast<D3D12Context *>(mThreadPool->GetContext(w));
                mParallelBuffers[w].resize(info.inflightNum);
                for (uint32_t f = 0; f < info.inflightNum; ++f) {
                    mParallelBuffers[w][f] = ctx->pool->Allocate();
                }
            }
        }
    }

    D3D12DeviceFrameContext::~D3D12DeviceFrameContext() noexcept
    {
        mThreadPool = nullptr;
        mPool       = nullptr;
        mD3D12Pool  = nullptr;
    }

} // namespace sky::aurora
