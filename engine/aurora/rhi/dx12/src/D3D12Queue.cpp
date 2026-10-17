//
// Aurora D3D12 Queue.
//

#include <D3D12CommandPool.h>
#include <D3D12Device.h>
#include <D3D12Fence.h>
#include <D3D12Queue.h>
#include <D3D12Semaphore.h>
#include <algorithm>
#include <aurora/rhi/SubmitInfo.h>
#include <core/logger/Logger.h>
#include <vector>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {

    D3D12Queue::D3D12Queue(D3D12Device &dev, QueueType t, ComPtr<ID3D12CommandQueue> q) : device(dev), type(t), queue(std::move(q))
    {
    }

    D3D12Queue::~D3D12Queue()
    {
        if (idleEvent != nullptr) {
            ::CloseHandle(idleEvent);
            idleEvent = nullptr;
        }
    }

    bool D3D12Queue::Init()
    {
        HRESULT hr = device.GetNativeHandle()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&idleFence));
        if (FAILED(hr)) {
            LOG_E(TAG, "queue idle fence creation failed: 0x%08x", hr);
            return false;
        }
        idleEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
        return idleEvent != nullptr;
    }

    void D3D12Queue::Submit(const SubmitInfo &info)
    {
        const auto result = TrySubmit(info);
        if (result.status != QueueSubmitStatus::ACCEPTED)
            LOG_E(TAG, "Queue submission failed: %lld", result.error.nativeCode);
    }

    QueueSubmitResult D3D12Queue::TrySubmit(const SubmitInfo &info) noexcept
    {
        bool attempted = false;

        if (info.commandBuffers.size() > UINT32_MAX)
            return {QueueSubmitStatus::REJECTED, {GpuErrorCode::INVALID_ARGUMENT}};
        std::vector<ID3D12CommandList *> lists;
        lists.reserve(info.commandBuffers.size());
        for (auto *buffer : info.commandBuffers) {
            if (!buffer || buffer->GetDevice() != &device)
                return {QueueSubmitStatus::REJECTED, {GpuErrorCode::INVALID_ARGUMENT}};
            auto *native = static_cast<D3D12CommandBuffer *>(buffer);
            if (!native->GetNativeHandle())
                return {QueueSubmitStatus::REJECTED, {GpuErrorCode::INVALID_ARGUMENT}};
            lists.push_back(native->GetNativeHandle());
        }
        for (const auto *semaphores : {&info.waitSemaphores, &info.signalSemaphores}) {
            for (const auto &entry : *semaphores) {
                if (!entry.semaphore || entry.semaphore->GetDevice() != &device)
                    return {QueueSubmitStatus::REJECTED, {GpuErrorCode::INVALID_ARGUMENT}};
                auto *native = static_cast<D3D12Semaphore *>(entry.semaphore);
                if ((native->GetType() == SemaphoreType::TIMELINE && entry.value == UINT64_MAX) ||
                    (native->GetType() == SemaphoreType::BINARY && native->GetBinaryWaitValue() >= UINT64_MAX - 1))
                    return {QueueSubmitStatus::REJECTED, {GpuErrorCode::INVALID_ARGUMENT}};
            }
        }
        if (info.fence) {
            if (info.fence->GetDevice() != &device)
                return {QueueSubmitStatus::REJECTED, {GpuErrorCode::INVALID_ARGUMENT}};
            auto *native = static_cast<D3D12Fence *>(info.fence);
            if (native->GetPendingValue() >= UINT64_MAX - 1)
                return {QueueSubmitStatus::REJECTED, {GpuErrorCode::INVALID_ARGUMENT}};
        }
        const auto failure = [&](HRESULT hr) {
            const auto lost = device.GetFunctions().removed(device.GetNativeHandle());
            return QueueSubmitResult{attempted ? QueueSubmitStatus::INDETERMINATE : QueueSubmitStatus::REJECTED,
                                     FAILED(lost) ? GpuError{GpuErrorCode::DEVICE_LOST, lost} : D3D12Error(hr)};
        };
        if (const auto lost = device.GetFunctions().removed(device.GetNativeHandle()); FAILED(lost))
            return failure(lost);

        // After the first native queue operation, any failure may leave partially enqueued work.
        for (const auto &w : info.waitSemaphores) {
            auto        *sema  = static_cast<D3D12Semaphore *>(w.semaphore);
            const UINT64 value = (sema->GetType() == SemaphoreType::TIMELINE) ? w.value : sema->GetBinaryWaitValue();
            attempted          = true;
            const auto hr      = device.GetFunctions().wait(queue.Get(), sema->GetNativeHandle(), value);
            if (FAILED(hr))
                return failure(hr);
        }

        if (!lists.empty()) {
            attempted = true;
            device.GetFunctions().execute(queue.Get(), static_cast<UINT>(lists.size()), lists.data());
        }

        for (const auto &s : info.signalSemaphores) {
            auto        *sema  = static_cast<D3D12Semaphore *>(s.semaphore);
            const UINT64 value = (sema->GetType() == SemaphoreType::TIMELINE) ? s.value : sema->AdvanceBinarySignalValue();
            attempted          = true;
            const auto hr      = device.GetFunctions().signal(queue.Get(), sema->GetNativeHandle(), value);
            if (FAILED(hr))
                return failure(hr);
        }

        if (info.fence != nullptr) {
            auto        *fence = static_cast<D3D12Fence *>(info.fence);
            const UINT64 v     = fence->BumpPendingValue();
            attempted          = true;
            const auto hr      = device.GetFunctions().signal(queue.Get(), fence->GetNativeHandle(), v);
            if (FAILED(hr))
                return failure(hr);
        }
        if (const auto lost = device.GetFunctions().removed(device.GetNativeHandle()); FAILED(lost))
            return failure(lost);
        return {QueueSubmitStatus::ACCEPTED};
    }

    void D3D12Queue::WaitIdle()
    {
        ++idleValue;
        // A failed signal must not lead to an infinite wait for an unreachable fence value.
        while (FAILED(device.GetFunctions().signal(queue.Get(), idleFence.Get(), idleValue))) {
            if (FAILED(device.GetFunctions().removed(device.GetNativeHandle())))
                return;
            ::Sleep(1);
        }
        if (idleFence->GetCompletedValue() < idleValue) {
            const auto eventResult = idleFence->SetEventOnCompletion(idleValue, idleEvent);
            if (SUCCEEDED(eventResult))
                ::WaitForSingleObject(idleEvent, INFINITE);
            else
                while (idleFence->GetCompletedValue() < idleValue)
                    ::Sleep(1);
        }
    }

    TransferTaskHandle D3D12Queue::UploadBuffer(Buffer *buffer, const std::vector<BufferUploadRequest> &requests)
    {
        auto *dst = static_cast<D3D12Buffer *>(buffer);
        if (dst == nullptr || requests.empty()) {
            return 0;
        }

        uint64_t total = 0;
        for (const auto &req : requests) {
            total += req.size;
        }

        Buffer::Descriptor stagingDesc = {};
        stagingDesc.size               = total;
        stagingDesc.usage              = BufferUsageFlagBit::TRANSFER_SRC;
        stagingDesc.memory             = MemoryType::CPU_TO_GPU;
        BufferPtr staging(device.CreateBuffer(stagingDesc));
        auto     *stagingDx = static_cast<D3D12Buffer *>(staging.Get());
        if (stagingDx == nullptr) {
            LOG_E(TAG, "failed to create staging buffer for upload");
            return 0;
        }

        uint8_t *mapped = stagingDx->Map();
        if (mapped == nullptr) {
            LOG_E(TAG, "failed to map staging buffer for upload");
            return 0;
        }
        uint64_t srcOffset = 0;
        for (const auto &req : requests) {
            req.source->ReadData(req.offset, req.size, mapped + srcOffset);
            srcOffset += req.size;
        }
        stagingDx->UnMap();

        auto *pool = device.CreateCommandPool(QueueType::TRANSFER);
        if (pool == nullptr) {
            LOG_E(TAG, "failed to create copy command pool for upload");
            return 0;
        }
        auto *cb = pool->Allocate();
        if (cb == nullptr) {
            delete pool;
            return 0;
        }
        cb->Begin();
        {
            auto     blit = cb->CreateBlitEncoder();
            uint64_t src  = 0;
            for (const auto &req : requests) {
                blit->CopyBuffer(stagingDx, dst, req.size, src, req.dstOffset);
                src += req.size;
            }
        }
        cb->End();

        Fence::Descriptor fenceDesc = {};
        fenceDesc.createSignaled    = false;
        FencePtr fence(device.CreateFence(fenceDesc));

        SubmitInfo submit;
        submit.commandBuffers.push_back(cb);
        submit.fence = fence.Get();
        Submit(submit); // submit to this (copy) queue, non-blocking

        PendingUpload pending;
        pending.fence   = std::move(fence);
        pending.staging = std::move(staging);
        pending.pool.reset(pool);

        pendingUploads.push_back(std::move(pending));
        return static_cast<TransferTaskHandle>(pendingUploads.size() - 1);
    }

    TransferTaskHandle D3D12Queue::UploadImage(Image *image, const std::vector<ImageUploadRequest> &requests)
    {
        auto *dst = static_cast<D3D12Image *>(image);
        if (dst == nullptr || requests.empty()) {
            return 0;
        }

        uint64_t total    = 0;
        uint32_t maxLevel = 0;
        uint32_t maxLayer = 0;
        for (const auto &req : requests) {
            total += req.size;
            maxLevel = std::max(maxLevel, req.mipLevel);
            maxLayer = std::max(maxLayer, req.layer);
        }

        Buffer::Descriptor stagingDesc = {};
        stagingDesc.size               = total;
        stagingDesc.usage              = BufferUsageFlagBit::TRANSFER_SRC;
        stagingDesc.memory             = MemoryType::CPU_TO_GPU;
        BufferPtr staging(device.CreateBuffer(stagingDesc));
        auto     *stagingDx = static_cast<D3D12Buffer *>(staging.Get());
        if (stagingDx == nullptr) {
            LOG_E(TAG, "failed to create staging buffer for image upload");
            return 0;
        }

        uint8_t *mapped = stagingDx->Map();
        if (mapped == nullptr) {
            LOG_E(TAG, "failed to map staging buffer for image upload");
            return 0;
        }
        uint64_t dstOffset = 0;
        for (const auto &req : requests) {
            req.source->ReadData(req.offset, req.size, mapped + dstOffset);
            dstOffset += req.size;
        }
        stagingDx->UnMap();

        auto *pool = device.CreateCommandPool(QueueType::TRANSFER);
        if (pool == nullptr) {
            LOG_E(TAG, "failed to create copy command pool for image upload");
            return 0;
        }
        auto *cb = pool->Allocate();
        if (cb == nullptr) {
            delete pool;
            return 0;
        }

        cb->Begin();

        BarrierInfo barrier;
        barrier.srcStage = PipelineStageBit::TOP;
        barrier.dstStage = PipelineStageBit::TRANSFER;
        ImageBarrierInfo imageBarrier;
        imageBarrier.image              = dst;
        imageBarrier.subRange.baseLevel = 0;
        imageBarrier.subRange.levels    = maxLevel + 1;
        imageBarrier.subRange.baseLayer = 0;
        imageBarrier.subRange.layers    = maxLayer + 1;
        imageBarrier.srcAccess          = AccessFlagBit::NONE;
        imageBarrier.dstAccess          = AccessFlagBit::COPY_DST;
        imageBarrier.oldLayout          = ImageLayout::UNDEFINED;
        imageBarrier.newLayout          = ImageLayout::TRANSFER_DST;
        barrier.imageBarriers.push_back(imageBarrier);
        cb->PipelineBarrier(barrier);

        {
            auto                         blit = cb->CreateBlitEncoder();
            std::vector<BufferImageCopy> regions(requests.size());
            uint64_t                     bufferOffset = 0;
            for (size_t i = 0; i < requests.size(); ++i) {
                const auto &req           = requests[i];
                auto       &region        = regions[i];
                region.bufferOffset       = bufferOffset;
                region.bufferRowLength    = req.bufferRowLength;
                region.bufferImageHeight  = req.bufferImageHeight;
                region.subRange.level     = req.mipLevel;
                region.subRange.baseLayer = req.layer;
                region.subRange.layers    = 1;
                region.imageOffset        = req.imageOffset;
                region.imageExtent        = req.imageExtent;
                bufferOffset += req.size;
            }
            blit->CopyBufferToImage(stagingDx, dst, regions);
        }

        BarrierInfo postBarrier;
        postBarrier.srcStage = PipelineStageBit::TRANSFER;
        postBarrier.dstStage = PipelineStageBit::BOTTOM;
        ImageBarrierInfo postImageBarrier;
        postImageBarrier.image              = dst;
        postImageBarrier.subRange.baseLevel = 0;
        postImageBarrier.subRange.levels    = maxLevel + 1;
        postImageBarrier.subRange.baseLayer = 0;
        postImageBarrier.subRange.layers    = maxLayer + 1;
        postImageBarrier.srcAccess          = AccessFlagBit::COPY_DST;
        postImageBarrier.dstAccess          = AccessFlagBit::NONE;
        postImageBarrier.oldLayout          = ImageLayout::TRANSFER_DST;
        postImageBarrier.newLayout          = ImageLayout::SHADER_READ_ONLY;
        postBarrier.imageBarriers.push_back(postImageBarrier);
        cb->PipelineBarrier(postBarrier);

        cb->End();

        Fence::Descriptor fenceDesc = {};
        fenceDesc.createSignaled    = false;
        FencePtr fence(device.CreateFence(fenceDesc));

        SubmitInfo submit;
        submit.commandBuffers.push_back(cb);
        submit.fence = fence.Get();
        Submit(submit);

        PendingUpload pending;
        pending.fence   = std::move(fence);
        pending.staging = std::move(staging);
        pending.pool.reset(pool);

        pendingUploads.push_back(std::move(pending));
        return static_cast<TransferTaskHandle>(pendingUploads.size() - 1);
    }

    void D3D12Queue::Wait(TransferTaskHandle handle)
    {
        if (handle >= pendingUploads.size()) {
            return;
        }
        auto &pending = pendingUploads[handle];
        if (pending.fence != nullptr) {
            pending.fence->Wait();
            pending.pool.reset();
            pending.staging = nullptr;
            pending.fence   = nullptr;
        }
    }

    bool D3D12Queue::HasComplete(TransferTaskHandle handle) const
    {
        if (handle >= pendingUploads.size()) {
            return true;
        }
        const auto &pending = pendingUploads[handle];
        return pending.fence == nullptr || pending.fence->IsSignaled();
    }

} // namespace sky::aurora
