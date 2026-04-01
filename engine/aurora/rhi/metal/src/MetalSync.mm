//
// Created on 2026/04/02.
//

#include <MetalSync.h>

namespace sky::aurora {

    bool MetalFence::Init(const Descriptor &desc)
    {
        signaled = desc.createSignaled;
        return true;
    }

    void MetalFence::Wait()
    {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [this]() { return signaled; });
    }

    void MetalFence::Reset()
    {
        std::lock_guard<std::mutex> lock(mutex);
        signaled = false;
    }

    bool MetalSemaphore::Init(const Descriptor &desc)
    {
        initialValue = desc.initialValue;
        return true;
    }

} // namespace sky::aurora