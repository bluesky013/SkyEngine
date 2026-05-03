//
// Aurora GLES Queue.
//

#include <GLESQueue.h>
#include <GLESDevice.h>
#include <GLESFence.h>
#include <GLESSemaphore.h>
#include <GLESLoader.h>
#include <aurora/rhi/SubmitInfo.h>

namespace sky::aurora {

    GLESQueue::GLESQueue(GLESDevice &dev)
        : device(dev)
    {
    }

    void GLESQueue::Submit(const SubmitInfo &info)
    {
        // 1. Wait on input semaphores (CPU-side block in this emulation).
        for (const auto &w : info.waitSemaphores) {
            auto *sema = static_cast<GLESSemaphore *>(w.semaphore);
            if (sema == nullptr) continue;
            const uint64_t target = (sema->GetType() == SemaphoreType::TIMELINE)
                                        ? w.value
                                        : sema->GetCurrentValue();
            // Block until counter reaches target. Single-threaded GLES emulation
            // means signals from earlier Submit calls have already happened.
            (void)sema->Wait(target, UINT64_MAX);
        }

        // 2. Cmdbuffers are no-ops on GLES (encoders executed immediately at record).

        // 3. Signal output semaphores.
        for (const auto &s : info.signalSemaphores) {
            auto *sema = static_cast<GLESSemaphore *>(s.semaphore);
            if (sema == nullptr) continue;
            const uint64_t value = (sema->GetType() == SemaphoreType::TIMELINE)
                                       ? s.value
                                       : (sema->GetCurrentValue() + 1);
            sema->Signal(value);
        }

        // 4. Signal fence.
        if (info.fence != nullptr) {
            static_cast<GLESFence *>(info.fence)->SignalFromQueue();
        }
    }

    void GLESQueue::WaitIdle()
    {
        glFinish();
    }

} // namespace sky::aurora
