//
// Created on 2026/04/01.
//

#include <GLESSemaphore.h>

namespace sky::aurora {

    bool GLESSemaphore::Init(const Descriptor &desc)
    {
        type    = desc.type;
        counter = (type == SemaphoreType::TIMELINE) ? desc.initialValue : 0;
        return true;
    }

    void GLESSemaphore::Signal(uint64_t value)
    {
        if (value > counter) {
            counter = value;
        }
    }

    bool GLESSemaphore::Wait(uint64_t value, uint64_t /*timeoutNs*/)
    {
        return counter >= value;
    }

} // namespace sky::aurora
