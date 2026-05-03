//
// Aurora GLES Queue (single logical queue; CPU-side sync emulation).
//

#pragma once

#include <aurora/rhi/Queue.h>

namespace sky::aurora {

    class GLESDevice;

    class GLESQueue : public Queue {
    public:
        explicit GLESQueue(GLESDevice &dev);
        ~GLESQueue() override = default;

        void      Submit(const SubmitInfo &info) override;
        void      WaitIdle() override;
        QueueType GetType() const override { return QueueType::GRAPHICS; }    // single logical queue

    private:
        GLESDevice &device;
    };

} // namespace sky::aurora
