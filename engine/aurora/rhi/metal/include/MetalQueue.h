//
// Aurora Metal Queue.
//

#pragma once

#include <aurora/rhi/Queue.h>

namespace sky::aurora {

    class MetalDevice;

    class MetalQueue : public Queue {
    public:
        MetalQueue(MetalDevice &dev, QueueType type, void *nativeQueue);
        ~MetalQueue() override;

        void      Submit(const SubmitInfo &info) override;
        void      WaitIdle() override;
        QueueType GetType() const override { return type; }

        void *GetNativeHandle() const { return queue; }    // id<MTLCommandQueue>

    private:
        MetalDevice &device;
        QueueType    type;
        void        *queue = nullptr;     // owned id<MTLCommandQueue>
    };

} // namespace sky::aurora
