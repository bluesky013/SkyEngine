//
// Aurora RHI Queue.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <vector>

namespace sky::aurora {

    struct SubmitInfo;
    class Buffer;
    class Image;

    class Queue {
    public:
        Queue() = default;
        virtual ~Queue() = default;

        virtual void      Submit(const SubmitInfo &info) = 0;
        virtual void      WaitIdle() = 0;
        virtual QueueType GetType() const = 0;

        // Asynchronous uploads: submit a staging copy to the transfer/upload
        // queue and return a waitable handle. The caller is responsible for
        // synchronizing (Wait/HasComplete) before the data is consumed.
        virtual TransferTaskHandle UploadBuffer(Buffer *buffer, const std::vector<BufferUploadRequest> &requests) = 0;
        virtual TransferTaskHandle UploadImage(Image *image, const std::vector<ImageUploadRequest> &requests) = 0;

        virtual void Wait(TransferTaskHandle handle) = 0;
        virtual bool HasComplete(TransferTaskHandle handle) const = 0;
    };

} // namespace sky::aurora
