//
// Created on 2026/04/02.
//

#include <MetalSync.h>
#include <chrono>

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

    bool MetalFence::IsSignaled()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return signaled;
    }

    bool MetalFence::WaitFor(uint64_t timeoutNs)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return condition.wait_for(lock, std::chrono::nanoseconds(timeoutNs), [this]() { return signaled; });
    }

    bool MetalSemaphore::Init(const Descriptor &desc)
    {
        type         = desc.type;
        initialValue = desc.initialValue;
        currentValue = desc.initialValue;
        return true;
    }

    void MetalSemaphore::Signal(uint64_t value)
    {
        if (value > currentValue) {
            currentValue = value;
        }
    }

    bool MetalSemaphore::Wait(uint64_t value, uint64_t /*timeoutNs*/)
    {
        return currentValue >= value;
    }

} // namespace sky::aurora
