//
// Created on 2026/04/01.
//

#include <GLESSemaphore.h>
#include <chrono>

namespace sky::aurora {

    bool GLESSemaphore::Init(const Descriptor &desc)
    {
        type    = desc.type;
        counter = (type == SemaphoreType::TIMELINE) ? desc.initialValue : 0;
        return true;
    }

    void GLESSemaphore::Signal(uint64_t value)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (value > counter) {
                counter = value;
            }
        }
        condition.notify_all();
    }

    bool GLESSemaphore::Wait(uint64_t value, uint64_t timeoutNs)
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (timeoutNs == UINT64_MAX) {
            condition.wait(lock, [this, value]() { return counter >= value; });
            return true;
        }
        return condition.wait_for(lock, std::chrono::nanoseconds(timeoutNs),
                                  [this, value]() { return counter >= value; });
    }

    uint64_t GLESSemaphore::GetCurrentValue() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return counter;
    }

} // namespace sky::aurora
