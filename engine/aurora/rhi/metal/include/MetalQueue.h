//
// Aurora Metal Queue.
//

#pragma once

#include <aurora/rhi/Queue.h>
#include <aurora/rhi/Buffer.h>
#include <aurora/rhi/Fence.h>

#include <memory>
#include <vector>

namespace sky::aurora {

    class MetalDevice;
    class CommandPool;

    class MetalQueue : public Queue {
    public:
        MetalQueue(MetalDevice &dev, QueueType type, void *nativeQueue);
        ~MetalQueue() override;

        void      Submit(const SubmitInfo &info) override;
        void      WaitIdle() override;
        QueueType GetType() const override { return type; }

        TransferTaskHandle UploadBuffer(Buffer *buffer, const std::vector<BufferUploadRequest> &requests) override;
        TransferTaskHandle UploadImage(Image *image, const std::vector<ImageUploadRequest> &requests) override;

        void Wait(TransferTaskHandle handle) override;
        bool HasComplete(TransferTaskHandle handle) const override;

        void *GetNativeHandle() const { return queue; }    // id<MTLCommandQueue>

    private:
        struct PendingUpload {
            FencePtr                     fence;
            BufferPtr                    staging;
            std::unique_ptr<CommandPool> pool;
        };

        MetalDevice &device;
        QueueType    type;
        void        *queue = nullptr;     // owned id<MTLCommandQueue>

        std::vector<PendingUpload> pendingUploads;
    };

} // namespace sky::aurora
